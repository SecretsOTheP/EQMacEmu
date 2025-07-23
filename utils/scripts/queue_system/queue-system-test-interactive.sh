#!/bin/bash

# EQMacEmu Queue System Interactive Test Script
# Comprehensive testing environment for queue system and auto-connect functionality
#
# IMPORTANT: Account-Based Population Tracking
# This script measures "Server Reservations" (unique account IDs with active reservations)
# NOT "Active Connections" (currently connected clients)
# 
# Server Reservations (account-based tracking) includes:
# - Players on character select screen
# - Players creating characters  
# - Players loading into zones
# - Players actively in zones
# - Players temporarily disconnected but within grace period
#
# This is the correct measure for queue decisions because:
# 1. Each account represents one server reservation/slot regardless of connection state
# 2. Prevents players from losing queue position due to crashes/disconnects
# 3. Accurately reflects server resource commitment vs just connection count
# 4. Handles zoning, loading, and temporary disconnections gracefully
# 5. More accurate than IP-based tracking (multiple accounts behind NAT, dynamic IPs)
#
# NEW DATABASE SCHEMA:
# - server_population: Updated every 15 seconds with total account reservation count (for real-time reporting)
# - active_account_connections: Updated every 5 minutes with detailed account reservation data (for crash recovery)
#   Contains: account_id, ip_address, last_seen, grace_period, is_in_raid
# - Grace periods: 60s (normal players), 600s (raid members)
#
# WORLD SERVER ID ISSUE FIX:
# In test environments with frequent server restarts, world servers may get new ServerIDs each time
# they restart, leaving orphaned queue entries tied to old server IDs. This script now:
# - Uses smarter auto-detection based on server names and queue history
# - Detects and warns about multiple server IDs in queue
# - Provides cleanup tools to remove orphaned entries (option 9)
# - Shows which server ID is currently active vs old/orphaned entries
#
# The script provides tools to monitor both tables and understand the dual-update system.

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
ORANGE='\033[38;5;208m'
PURPLE='\033[0;35m'
WHITE='\033[1;37m'
NC='\033[0m' # No Color

# ============================================================================
# USER CONFIGURATION - Modify these settings for your setup
# ============================================================================

# Path to eqemu_config.json (modify if your config lives elsewhere)
CONFIG_PATH="$HOME/quick-quarm/bin/eqemu_config.json"

# Attempt to read DB credentials from config using jq; fallback to defaults
if [[ -f "$CONFIG_PATH" ]]; then
    # Read database settings using native tools (no jq required)
    if command -v jq >/dev/null 2>&1; then
        # Use jq if available (faster and more reliable)
    DB_HOST=$(jq -r '.database.host' "$CONFIG_PATH")
    DB_USER=$(jq -r '.database.username' "$CONFIG_PATH")
    DB_PASS=$(jq -r '.database.password' "$CONFIG_PATH")
    DB_NAME=$(jq -r '.database.db' "$CONFIG_PATH")
        WORLD_SERVER_NAME=$(jq -r '.longname' "$CONFIG_PATH")
        WORLD_SERVER_SHORT_NAME=$(jq -r '.shortname' "$CONFIG_PATH")
    else
        # Use native bash tools to parse JSON - extract database section first
        # Get the database section between "database": { and the closing }
        DB_SECTION=$(sed -n '/"database"[[:space:]]*:[[:space:]]*{/,/^[[:space:]]*}/p' "$CONFIG_PATH")
        
        # Extract database values from the database section
        DB_HOST=$(echo "$DB_SECTION" | grep -o '"host"[[:space:]]*:[[:space:]]*"[^"]*"' | head -1 | sed 's/.*"host"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/')
        DB_USER=$(echo "$DB_SECTION" | grep -o '"username"[[:space:]]*:[[:space:]]*"[^"]*"' | head -1 | sed 's/.*"username"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/')
        DB_PASS=$(echo "$DB_SECTION" | grep -o '"password"[[:space:]]*:[[:space:]]*"[^"]*"' | head -1 | sed 's/.*"password"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/')
        DB_NAME=$(echo "$DB_SECTION" | grep -o '"db"[[:space:]]*:[[:space:]]*"[^"]*"' | head -1 | sed 's/.*"db"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/')
        
        # Extract server names from root level (not in database section)
        WORLD_SERVER_NAME=$(grep -o '"longname"[[:space:]]*:[[:space:]]*"[^"]*"' "$CONFIG_PATH" | head -1 | sed 's/.*"longname"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/')
        WORLD_SERVER_SHORT_NAME=$(grep -o '"shortname"[[:space:]]*:[[:space:]]*"[^"]*"' "$CONFIG_PATH" | head -1 | sed 's/.*"shortname"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/')
    fi
    
    # Set defaults for any empty values
    DB_HOST=${DB_HOST:-"localhost"}
    DB_USER=${DB_USER:-"quarm"}
    DB_PASS=${DB_PASS:-"quarm"}
    DB_NAME=${DB_NAME:-"quarm"}
    WORLD_SERVER_NAME=${WORLD_SERVER_NAME:-"Quick Quarm EQ"}
    WORLD_SERVER_SHORT_NAME=${WORLD_SERVER_SHORT_NAME:-"QuickQuarm"}
    
    # Validate that we got valid values (not null or empty)
    if [ "$WORLD_SERVER_NAME" = "null" ] || [ -z "$WORLD_SERVER_NAME" ]; then
        WORLD_SERVER_NAME="Quick Quarm EQ"  # Fallback
    fi
    if [ "$WORLD_SERVER_SHORT_NAME" = "null" ] || [ -z "$WORLD_SERVER_SHORT_NAME" ]; then
        WORLD_SERVER_SHORT_NAME="QuickQuarm"  # Fallback
    fi
else
    # Fallback / manual override
    DB_HOST="localhost"
    DB_USER="quarm"
    DB_PASS="quarm"
    DB_NAME="quarm"
    WORLD_SERVER_NAME="Quick Quarm EQ"
    WORLD_SERVER_SHORT_NAME="QuickQuarm"
fi

# Server details - Now automatically read from eqemu_config.json
# These values are read from your bin/eqemu_config.json "longname" and "shortname" fields
# If config file is not found or jq is not installed, defaults to "Quick Quarm EQ" / "QuickQuarm"

# Alternative: Auto-detect first available server (set to "true" to enable)
AUTO_DETECT_SERVER="true"

# ============================================================================
# END USER CONFIGURATION
# ============================================================================

# Function to check database prerequisites
check_database_prerequisites() {
    print_step "Checking database prerequisites for queue system..."
    
    # Check if SQL migration file exists
    if [ ! -f "$SQL_MIGRATION_PATH" ]; then
        print_error "Required SQL migration file not found:"
        print_error "  Expected: $SQL_MIGRATION_PATH"
        print_error "  This file is required to create queue system tables."
        return 1
    fi
    
    print_info "✓ SQL migration file found: $SQL_MIGRATION_PATH"
    
    # Check if required tables exist
    local required_tables=("tblLoginQueue" "server_population" "tblloginserversettings")
    local missing_tables=()
    
    for table in "${required_tables[@]}"; do
        if ! mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -e "DESCRIBE $table;" >/dev/null 2>&1; then
            missing_tables+=("$table")
        fi
    done
    
    if [ ${#missing_tables[@]} -gt 0 ]; then
        print_error "Missing required database tables: ${missing_tables[*]}"
        print_error "Please run the SQL migration first:"
        print_error "  mysql -h$DB_HOST -u$DB_USER -p$DB_PASS $DB_NAME < \"$SQL_MIGRATION_PATH\""
        echo
        print_info "You can also run the migration automatically:"
        echo -n "Run SQL migration now? [y/N]: "
        read run_migration
        if [[ "$run_migration" =~ ^[Yy]$ ]]; then
            print_step "Running SQL migration..."
            if mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" < "$SQL_MIGRATION_PATH" 2>/dev/null; then
                print_info "✓ SQL migration completed successfully"
            else
                print_error "✗ SQL migration failed - please run manually"
                return 1
            fi
        else
            return 1
        fi
    else
        print_info "✓ All required database tables exist"
    fi
    
    # Check if queue rules exist with new naming convention
    local queue_rules=("Quarm.PlayerPopulationCap" "Quarm.EnableQueue" "Quarm.TestPopulationOffset")
    local missing_rules=()
    
    for rule in "${queue_rules[@]}"; do
        local rule_exists=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT COUNT(*) FROM rule_values WHERE rule_name='$rule';" 2>/dev/null)
        if [ "$rule_exists" = "0" ]; then
            missing_rules+=("$rule")
        fi
    done
    
    if [ ${#missing_rules[@]} -gt 0 ]; then
        print_error "Missing queue system rules: ${missing_rules[*]}"
        print_error "These will be created automatically when you run setup option (10)."
        print_info "Note: Queue system uses new Quarm.* rule naming convention as of 2025"
    else
        print_info "✓ Queue system rules found with correct naming convention"
    fi
    
    print_info "✓ Database prerequisites check completed"
    return 0
}

print_header() {
    echo -e "${BLUE}============================================${NC}"
    echo -e "${BLUE}  EQMacEmu Queue System Test Infrastructure${NC}"
    echo -e "${BLUE}============================================${NC}"
    echo
}

print_step() {
    echo -e "${GREEN}[STEP]${NC} $1"
}

print_info() {
    echo -e "${YELLOW}[INFO]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Execute SQL command with validation
execute_sql() {
    local sql_command="$1"
    local result=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -e "$sql_command" 2>&1)
    local exit_code=$?
    
    if [ $exit_code -ne 0 ]; then
        echo -e "${RED}[ERROR]${NC} SQL execution failed: $result"
        return 1
    fi
    
    echo "$result"
    return 0
}

# Trigger queue refresh by setting database flag
trigger_queue_refresh() {
    mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -e "
INSERT INTO tblloginserversettings (type, value, category, description) 
VALUES ('RefreshQueue', '1', 'options', 'Trigger queue refresh - auto-reset by system') 
ON DUPLICATE KEY UPDATE value = '1';
" 2>/dev/null

    if [ $? -eq 0 ]; then
        echo -e "${GREEN}[INFO]${NC} Queue refresh triggered - live system will sync within seconds"
    else
        echo -e "${YELLOW}[WARN]${NC} Failed to trigger queue refresh"
    fi
}

# Function to find the current world server ID
find_world_server_id() {
    local quiet_flag="$1"
    
    if [ "$quiet_flag" != "quiet" ]; then
        print_info "Auto-detecting world server..."
    fi
    
    # Get the most recent server registration regardless of name
    # This is more reliable than name matching
    local latest_server_id=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "
                SELECT ServerID 
                FROM tblWorldServerRegistration 
                ORDER BY ServerID DESC 
                LIMIT 1;" 2>/dev/null)
    
    if [ -n "$latest_server_id" ] && [ "$latest_server_id" -ne 0 ]; then
        # Get the server name for verification
        local server_name=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "
            SELECT ServerLongName 
            FROM tblWorldServerRegistration 
            WHERE ServerID = $latest_server_id;" 2>/dev/null)
        
        if [ "$quiet_flag" != "quiet" ]; then
            print_info "Found current server '$server_name' with ID: $latest_server_id"
        fi
        
        echo "$latest_server_id"
        return 0
    else
        if [ "$quiet_flag" != "quiet" ]; then
            print_error "No world server registrations found"
        fi
        echo "0"
        return 1
    fi
}

# Function to show available world servers (for troubleshooting)
show_available_servers() {
    print_info "Available world servers in database:"
    mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -e "SELECT ServerID, serverLongName, serverShortName FROM tblWorldServerRegistration;" 2>/dev/null
    echo
    print_info "If no servers are listed, make sure your world server is running and has registered with the login server."
    print_info "Check your eqemu_config.json 'longname' and 'shortname' settings."
}

# Function to check if rule exists and update it
set_rule() {
    local rule_name="$1"
    local rule_value="$2"
    
    # Check if rule exists
    local exists=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -se "SELECT COUNT(*) FROM rule_values WHERE ruleset_id=1 AND rule_name='$rule_name';" 2>/dev/null)
    
    if [ "$exists" = "1" ]; then
        execute_sql "UPDATE rule_values SET rule_value='$rule_value' WHERE ruleset_id=1 AND rule_name='$rule_name';"
        print_info "Updated rule $rule_name = $rule_value"
    else
        execute_sql "INSERT INTO rule_values (ruleset_id, rule_name, rule_value, notes) VALUES (1, '$rule_name', '$rule_value', 'Test queue system');"
        print_info "Created rule $rule_name = $rule_value"
    fi
}

# Function to create test accounts
create_test_accounts() {
    print_step "Creating test accounts for queue simulation..."
    
    for i in {1..10}; do
        local account_name="queuetest$i"
        local exists=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -se "SELECT COUNT(*) FROM tblLoginServerAccounts WHERE AccountName='$account_name';" 2>/dev/null)
        
        if [ "$exists" = "0" ]; then
            execute_sql "INSERT INTO tblLoginServerAccounts (AccountName, AccountPassword, AccountEmail, LastLoginDate, LastIPAddress) VALUES ('$account_name', SHA('testpass'), 'test$i@test.com', NOW(), '127.0.0.1');"
            print_info "Created test account: $account_name"
        fi
    done
}

# Function to setup queue environment
setup_queue_environment() {
    print_step "Setting up queue test environment..."
    
    # Set very low population cap for testing
    set_rule "Quarm.PlayerPopulationCap" "5"
    
    # Enable queue system
    set_rule "Quarm.EnableQueue" "true"
    set_rule "Quarm.EnableQueueLogging" "true"
    set_rule "Quarm.QueueEstimatedWaitPerPlayer" "30"
    
    print_info "Queue environment configured:"
    print_info "  - Population cap: 5 players"
    print_info "  - Queue system: Enabled"
    print_info "  - Wait time: 30 seconds per position"
}

# Function to simulate population
simulate_population() {
    local count="$1"
    print_step "Simulating $count online players..."
    
    # This would typically be done by having actual clients connected
    # For testing, we can temporarily modify the status reporting
    print_info "To simulate population, you'll need to:"
    print_info "  1. Connect $count test clients to the world server"
    print_info "  2. Or use the simulate-population.sh script"
    print_info "  3. Current simulation creates accounts ready for queue testing"
}

# Function to create fake queue entries for testing
create_fake_queue() {
    print_step "Creating simulated queue entries..."
    
    # Get world server ID (assuming first/only server for testing)
    local world_server_id=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -se "SELECT ServerID FROM tblWorldServerRegistration ORDER BY ServerID LIMIT 1;" 2>/dev/null)
    
    if [ -z "$world_server_id" ]; then
        print_error "No world server found in database. Make sure world server is registered."
        return 1
    fi
    
    print_info "Using world server ID: $world_server_id"
    
    # Clear existing queue entries
    execute_sql "DELETE FROM tblLoginQueue WHERE world_server_id = $world_server_id;"
    
    # Create queue entries for test accounts
    for i in {1..8}; do
        local account_id=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -se "SELECT LoginServerID FROM tblLoginServerAccounts WHERE AccountName='queuetest$i';" 2>/dev/null)
        
        if [ -n "$account_id" ]; then
            local position=$((10 - i))  # Positions 9, 8, 7, 6, 5, 4, 3, 2
            local wait_time=$((position * 30))
            
            execute_sql "INSERT INTO tblLoginQueue (account_id, world_server_id, queue_position, estimated_wait, ip_address, queued_timestamp, last_updated) VALUES ($account_id, $world_server_id, $position, $wait_time, INET_ATON('127.0.0.1'), NOW(), NOW());"
            print_info "Added queuetest$i to queue at position $position"
        fi
    done
    
    print_info "Queue simulation created with 8 players in positions 2-9"
    print_info "Position 1 is reserved for your test account"
}

# Function to set population offset
set_population_offset() {
    print_step "Setting Population Offset..."
    
    # Get current offset value
    local current_offset=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT rule_value FROM rule_values WHERE rule_name IN ('Quarm.TestPopulationOffset', 'Quarm:TestPopulationOffset') LIMIT 1;" 2>/dev/null)
    current_offset=${current_offset:-0}
    
    print_info "Current Test Population Offset: $current_offset"
    echo
    echo "Enter new population offset value (0-1200):"
    echo "  0   = No offset (real population only)"
    echo "  100 = Add 100 fake players"
    echo "  500 = Add 500 fake players"
    echo
    echo -n "New offset value: "
    read new_offset
    
    # Validate input
    if [[ ! "$new_offset" =~ ^[0-9]+$ ]]; then
        print_error "Invalid input. Please enter a number."
        return 1
    fi
    
    if [ "$new_offset" -lt 0 ] || [ "$new_offset" -gt 1200 ]; then
        print_error "Offset must be between 0 and 1200."
        return 1
    fi
    
    # Update the database - try both naming conventions
    execute_sql "UPDATE rule_values SET rule_value='$new_offset' WHERE rule_name='Quarm.TestPopulationOffset';"
    execute_sql "UPDATE rule_values SET rule_value='$new_offset' WHERE rule_name='Quarm:TestPopulationOffset';"
    
    # Create the rule if it doesn't exist
    execute_sql "INSERT IGNORE INTO rule_values (ruleset_id, rule_name, rule_value, notes) VALUES (1, 'Quarm:TestPopulationOffset', '$new_offset', 'Test population offset for queue testing');"
    
    print_info "Population offset updated: $current_offset → $new_offset"
    print_info "The world server will update the total population count within ~15 seconds."
    
    if [ "$new_offset" -gt 0 ]; then
        print_info "This will add $new_offset fake players to the server population for testing."
    else
        print_info "Test offset disabled - showing real population only."
    fi
}

# Function to show current queue status
show_queue_status() {
    print_step "Current Queue Status:"
    
    local world_server_id=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -se "SELECT ServerID FROM tblWorldServerRegistration ORDER BY ServerID LIMIT 1;" 2>/dev/null)
    
    if [ -z "$world_server_id" ]; then
        print_error "No world server found in database."
        print_info "Make sure both servers are running."
        return 1
    fi
    
    echo
    echo "Position | Account     | Acc ID | ETA       | Timestamp"
    echo "---------|-------------|--------|-----------|-------------------"
    
    mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -se "
        SELECT 
            lq.queue_position,
            lsa.AccountName,
            lq.estimated_wait,
            lq.last_updated
        FROM tblLoginQueue lq
        JOIN tblLoginServerAccounts lsa ON lq.account_id = lsa.LoginServerID
        WHERE lq.world_server_id = $world_server_id
        ORDER BY lq.queue_position;
    " | while read position account wait_time timestamp; do
        printf "%-8s | %-11s | %-9s | %s\n" "$position" "$account" "${wait_time}s" "$timestamp"
    done
    
    echo
    local queue_count=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -se "SELECT COUNT(*) FROM tblLoginQueue WHERE world_server_id = $world_server_id;" 2>/dev/null)
    echo "Total players in queue: $queue_count"
}

# Function for live queue monitoring
live_queue_monitor() {
    print_step "Starting Live Queue Monitor..."
    print_info "Press Ctrl+C to return to main menu"
    echo

    # Clear entire screen and reset cursor position
    clear
    
    # Initialize queue mode (1 = Dynamic, 2 = Chrono)
    local queue_mode=1
    local notification=""

    # Hide cursor for smoother redraws
    tput civis
    
    # Save the original trap and set local trap for monitor
    local original_trap=$(trap -p INT)
    trap 'echo -e "\n${YELLOW}[INFO]${NC} Returning to main menu..."; restore_main_trap; tput cnorm; return 0' INT
    
    # Test database connection first
    if ! mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -e "SELECT 1;" >/dev/null 2>&1; then
        print_error "Cannot connect to database. Check connection settings at top of script."
        restore_main_trap
        tput cnorm
        return 1
    fi
    
    while true; do
        # Clear screen properly using clear command instead of escape sequences
        clear
        
        echo -e "${BLUE}=== LIVE QUEUE MONITOR (Press Ctrl+C to exit) ===${NC}"
        
        # Show notification if any (only once)
        if [ -n "$notification" ]; then
            echo -e "${YELLOW}${notification}${NC}"
            notification=""  # Clear so it shows only once
        fi
        echo
        
        # Get current population and settings
        local real_population=$(get_real_player_count)
        local client_count=$(get_client_count)
        local detailed_account_count=$(get_detailed_account_count)
        local test_offset=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT rule_value FROM rule_values WHERE rule_name = 'Quarm:TestPopulationOffset';" 2>/dev/null)
        local max_players=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT rule_value FROM rule_values WHERE rule_name = 'Quarm:PlayerPopulationCap';" 2>/dev/null)
        local queue_enabled=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT rule_value FROM rule_values WHERE rule_name = 'Quarm:EnableQueue';" 2>/dev/null)
        
        # If colon notation doesn't work, try dot notation as fallback
        if [ -z "$test_offset" ]; then
            test_offset=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT rule_value FROM rule_values WHERE rule_name = 'Quarm.TestPopulationOffset';" 2>/dev/null)
        fi
        if [ -z "$max_players" ]; then
            max_players=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT rule_value FROM rule_values WHERE rule_name = 'Quarm.PlayerPopulationCap';" 2>/dev/null)
        fi
        if [ -z "$queue_enabled" ]; then
            queue_enabled=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT rule_value FROM rule_values WHERE rule_name = 'Quarm.EnableQueue';" 2>/dev/null)
        fi
        
        # Set defaults if values are empty
        real_population=${real_population:-0}
        client_count=${client_count:-0}
        detailed_account_count=${detailed_account_count:-0}
        test_offset=${test_offset:-0}
        max_players=${max_players:-1200}
        queue_enabled=${queue_enabled:-false}
        
        # The real_population already includes test offset from world server, so use it directly
        local effective_population=$real_population
        
        # Calculate how much of the population is from test offset vs real accounts
        local actual_accounts=$((real_population - test_offset))
        if [ $actual_accounts -lt 0 ]; then
            actual_accounts=0
        fi
        
        # Determine queue status
        local queue_status="DISABLED"
        local queue_active="INACTIVE"
        if [ "$queue_enabled" = "true" ]; then
            queue_status="ENABLED"
            if [ $effective_population -ge $max_players ]; then
                queue_active="ACTIVE (queuing new connections)"
            else
                queue_active="INACTIVE (server has capacity)"
            fi
        fi
        
        # Show population stats with both metrics
        echo -e "${GREEN}Server Status:${NC}"
        echo "  Server Capacity: $max_players"
        echo "  Queue System: $queue_status"
        echo "  Queue Status: $queue_active"
        
        # Show queue configuration
        echo -e "${YELLOW}Queue Configuration:${NC}"
        
        # Check if any of our expected rules exist
        local rule_count=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "
            SELECT COUNT(*) FROM rule_values 
            WHERE rule_name IN (
                'Quarm.EnableQueue',
                'Quarm.PlayerPopulationCap', 
                'Quarm.TestPopulationOffset',
                'Quarm.QueueBypassGMLevel',
                'Quarm.QueueEstimatedWaitPerPlayer'
            );" 2>/dev/null)
        
        # Also check for colon-based naming convention
        local rule_count_colon=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "
            SELECT COUNT(*) FROM rule_values 
            WHERE rule_name IN (
                'Quarm:EnableQueue',
                'Quarm:PlayerPopulationCap', 
                'Quarm:TestPopulationOffset',
                'Quarm:QueueBypassGMLevel',
                'Quarm:QueueEstimatedWaitPerPlayer'
            );" 2>/dev/null)
        
        if [ "$rule_count" -gt 0 ]; then
            mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "
                SELECT 
                    CONCAT('  ', 
                        CASE 
                            WHEN rule_name = 'Quarm.EnableQueue' THEN 'Queue Enabled: '
                            WHEN rule_name = 'Quarm.PlayerPopulationCap' THEN 'Max Players: '
                            WHEN rule_name = 'Quarm.TestPopulationOffset' THEN 'Test Offset: '
                            WHEN rule_name = 'Quarm.QueueBypassGMLevel' THEN 'GM Bypass: '
                            WHEN rule_name = 'Quarm.QueueEstimatedWaitPerPlayer' THEN 'Wait Per Player: '
                            ELSE CONCAT(SUBSTRING(rule_name, 7), ': ')
                        END,
                        rule_value,
                        CASE 
                            WHEN rule_name = 'Quarm.QueueEstimatedWaitPerPlayer' THEN ' seconds'
                            ELSE ''
                        END
                    )
                FROM rule_values 
                WHERE rule_name IN (
                    'Quarm.EnableQueue',
                    'Quarm.PlayerPopulationCap', 
                    'Quarm.TestPopulationOffset',
                    'Quarm.QueueBypassGMLevel',
                    'Quarm.QueueEstimatedWaitPerPlayer'
                )
                ORDER BY FIELD(rule_name, 
                    'Quarm.EnableQueue',
                    'Quarm.PlayerPopulationCap', 
                    'Quarm.TestPopulationOffset',
                    'Quarm.QueueBypassGMLevel',
                    'Quarm.QueueEstimatedWaitPerPlayer'
                );
            " 2>/dev/null
        elif [ "$rule_count_colon" -gt 0 ]; then
            mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "
                SELECT 
                    CONCAT('  ', 
                        CASE 
                            WHEN rule_name = 'Quarm:EnableQueue' THEN 'Queue Enabled: '
                            WHEN rule_name = 'Quarm:PlayerPopulationCap' THEN 'Max Players: '
                            WHEN rule_name = 'Quarm:TestPopulationOffset' THEN 'Test Offset: '
                            WHEN rule_name = 'Quarm:QueueBypassGMLevel' THEN 'GM Bypass: '
                            WHEN rule_name = 'Quarm:QueueEstimatedWaitPerPlayer' THEN 'Wait Per Player: '
                            ELSE CONCAT(SUBSTRING(rule_name, 7), ': ')
                        END,
                        rule_value,
                        CASE 
                            WHEN rule_name = 'Quarm:QueueEstimatedWaitPerPlayer' THEN ' seconds'
                            ELSE ''
                        END
                    )
                FROM rule_values 
                WHERE rule_name IN (
                    'Quarm:EnableQueue',
                    'Quarm:PlayerPopulationCap', 
                    'Quarm:TestPopulationOffset',
                    'Quarm:QueueBypassGMLevel',
                    'Quarm:QueueEstimatedWaitPerPlayer'
                )
                ORDER BY FIELD(rule_name, 
                    'Quarm:EnableQueue',
                    'Quarm:PlayerPopulationCap', 
                    'Quarm:TestPopulationOffset',
                    'Quarm:QueueBypassGMLevel',
                    'Quarm:QueueEstimatedWaitPerPlayer'
                );
            " 2>/dev/null
        else
            echo "  Queue rules not found in database"
        fi
        
        # Show average wait time as N/A
        echo "  Average Wait Time: N/A"
        
        # Display test offset for easier reading
        echo
        echo -e "${GREEN}Test Offset:${NC}"
        echo "  +$test_offset"
        
        # Show commands at the top so they don't get scrolled away
        echo
        echo -e "${PURPLE}Commands:${NC}"
        echo -e "${PURPLE}  [${WHITE}6${PURPLE}] Delete queue         [${WHITE}8${PURPLE}] Toggle queue mode    [${WHITE}p${PURPLE}] Set population${NC}"
        echo -e "${PURPLE}  [${WHITE}s${PURPLE}] Lower pop (-1)       [${WHITE}w${PURPLE}] Increase pop (+1)    [${WHITE}q${PURPLE}] Quit${NC}"
        echo -e "${PURPLE}  [${WHITE}x${PURPLE}] Rescan server${NC}"
if [ "$queue_mode" = "1" ]; then
    echo -e "${PURPLE}  Queue mode: ${ORANGE}**(1) Dynamic - Rotate RAID, NORM, GRP, NEWB**${PURPLE} | (2) Chrono - Time based${NC}"
else
    echo -e "${PURPLE}  Queue mode: (1) Dynamic - Rotate RAID, NORM, GRP, NEWB | ${ORANGE}**(2) Chrono - Time based**${PURPLE}${NC}"
fi
        
        # Check for queue entries
        echo
        local queue_count=0
        
        if mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -e "DESCRIBE tblLoginQueue;" >/dev/null 2>&1; then
            queue_count=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT COUNT(*) FROM tblLoginQueue;" 2>/dev/null)
            queue_count=${queue_count:-0}
            
            echo -e "${YELLOW}Current Queue Entries:${NC}"
            
            # Just find server IDs that actually have queue data - much simpler!
            local servers_with_queues=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "
                SELECT DISTINCT world_server_id 
                FROM tblLoginQueue 
                ORDER BY world_server_id DESC;" 2>/dev/null)
            
            if [ -n "$servers_with_queues" ]; then
                # Use the most recent server ID (highest number) as primary display
                local primary_server_id=$(echo "$servers_with_queues" | head -1)
                echo -e "${BLUE}Showing queue entries for server ID: $primary_server_id${NC}"
                
                # Show how many servers have queue data
                local server_count=$(echo "$servers_with_queues" | wc -l)
                if [ "$server_count" -gt 1 ]; then
                    echo -e "${YELLOW}📝 Note: Found queue entries on $server_count servers: $(echo $servers_with_queues | tr '\n' ' ')${NC}"
                    echo -e "${YELLOW}   Displaying entries from most recent server ($primary_server_id). Use option [6] to clean up old servers.${NC}"
                fi
            else
                echo -e "${BLUE}No queue entries found in database${NC}"
                primary_server_id=""
            fi
            
            # Debug information
            local total_entries=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "
                SELECT COUNT(*) FROM tblLoginQueue;" 2>/dev/null)
            local current_entries=0
            if [ -n "$primary_server_id" ]; then
                current_entries=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "
                    SELECT COUNT(*) FROM tblLoginQueue WHERE world_server_id = $primary_server_id;" 2>/dev/null)
            fi
            
            echo "Debug: Total queue entries: ${total_entries:-0} | For display server: ${current_entries:-0}"
            
            echo "Position | Account     | Acc ID | ETA       | Flag | Last Online         | In Queue | Server | IP Address"
            echo "---------|-------------|--------|-----------|------|---------------------|----------|--------|---------------"
            
            if [ $queue_count -gt 0 ]; then
                # Show queue entries for the server we determined has entries
                local query_filter=""
                if [ -n "$primary_server_id" ]; then
                    query_filter="WHERE lq.world_server_id = $primary_server_id"
                fi
                
                # Simplified queue entry display - don't try complex account lookups in the loop
                mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "
                    SELECT 
                        lq.queue_position,
                        COALESCE(a.name, CONCAT('Account-', lq.account_id)) as account_name,
                        lq.account_id,
                        'N/A',
                        'NORM' as flag,
                        'unknown' as last_online,
                        TIME_FORMAT(SEC_TO_TIME(TIMESTAMPDIFF(SECOND, lq.last_updated, NOW())), '%i:%s') as in_queue,
                        lq.world_server_id,
                        CONCAT(
                            (lq.ip_address & 0xFF), '.',
                            ((lq.ip_address >> 8) & 0xFF), '.',
                            ((lq.ip_address >> 16) & 0xFF), '.',
                            ((lq.ip_address >> 24) & 0xFF)
                        ) as ip_addr
                    FROM tblLoginQueue lq
                    LEFT JOIN account a ON lq.account_id = a.id
                    $query_filter
                    ORDER BY lq.world_server_id, lq.queue_position LIMIT 20;
                " 2>/dev/null | while IFS=$'\t' read position account acc_id wait_time flag last_online in_queue server_id ip_addr; do
                    printf "%-8s | %-11s | %-6s | %-9s | %-4s | %-19s | %-8s | %-6s | %s\n" \
                        "$position" "$account" "$acc_id" "$wait_time" "$flag" "$last_online" "$in_queue" "$server_id" "$ip_addr"
                done
                
                # Show raw debug info if enabled - always show when requested
                if [ "${show_raw_queue:-false}" = "true" ]; then
                    echo
                    echo -e "${BLUE}Raw queue entries in database (all servers):${NC}"
                    if [ "$queue_count" -gt 0 ]; then
                        mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" --table -e "SELECT * FROM tblLoginQueue ORDER BY world_server_id, queue_position LIMIT 20;" 2>/dev/null
                    else
                        echo "   (no queue entries in database)"
                    fi
                fi
                
                # Check if we have entries but they're not showing in the formatted display
                local displayed_count=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "
                    SELECT COUNT(*) FROM tblLoginQueue lq $query_filter;" 2>/dev/null)
                displayed_count=${displayed_count:-0}  # Default to 0 if empty
                
                if [ "$displayed_count" -eq 0 ]; then
                    # No entries for current server, but entries exist elsewhere
                    if [ "$queue_count" -gt 0 ]; then
                        echo -e "${YELLOW}   (no entries for display server $primary_server_id - $queue_count total entries may be on other servers)${NC}"
                        echo -e "${YELLOW}   Tip: Use option [6] to clean up old queue entries or check login server logs${NC}"
                    else
                        echo -e "${YELLOW}   (no queue entries found in database)${NC}"
                    fi
                fi
            else
                echo "   (empty queue)"
            fi
        else
            echo -e "${YELLOW}Queue table not found - queue system may not be configured${NC}"
        fi
        
        # Show refresh info
        echo
        echo -e "${BLUE}Last updated: $(date '+%H:%M:%S') | Refreshing every 3 seconds...${NC}"
        
        # Wait for input with timeout (3 seconds)
        # Use read with timeout and handle key presses
        read -t 3 -n 1 key 2>/dev/null
        
        case "$key" in
            # REMOVED: Cases "1" through "5" - force login, boot from queue, move up/down queue, send dialog msg
            "6")
                # Delete entire queue
                echo
                echo -e "${YELLOW}Available servers with queue entries:${NC}"
                mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" --table -e "
                    SELECT 
                        lq.world_server_id as 'Server ID',
                        COUNT(*) as 'Queue Count',
                        MAX(lq.last_updated) as 'Last Updated'
                    FROM tblLoginQueue lq
                    GROUP BY lq.world_server_id
                    ORDER BY lq.last_updated DESC;" 2>/dev/null
                
                echo
                echo -n -e "${YELLOW}Enter server ID to clear (or 'all' for all servers): ${NC}"
                read server_choice
                
                if [ "$server_choice" = "all" ]; then
                    echo
                    echo -e "${RED}WARNING: This will delete the entire queue for ALL world servers!${NC}"
                    echo -n -e "${YELLOW}Are you absolutely sure? [y/N]: ${NC}"
                    read -n 1 confirm
                    echo
                    if [[ "$confirm" =~ ^[Yy]$ ]]; then
                        local total_queue_count=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT COUNT(*) FROM tblLoginQueue;" 2>/dev/null)
                        
                        if [ "$total_queue_count" -gt 0 ]; then
                            execute_sql "DELETE FROM tblLoginQueue;"
                            local delete_result=$?
                            
                            if [ $delete_result -eq 0 ]; then
                                notification="${GREEN}Deleted entire queue: removed $total_queue_count players from all servers${NC}"
                                trigger_queue_refresh
                            else
                                notification="${RED}Failed to delete queue - MySQL error code: $delete_result${NC}"
                            fi
                        else
                            notification="${YELLOW}Queue is already empty - nothing to delete${NC}"
                        fi
                    else
                        notification="${BLUE}Queue deletion cancelled${NC}"
                    fi
                elif [ -n "$server_choice" ] && [[ "$server_choice" =~ ^[0-9]+$ ]]; then
                    echo
                    local server_queue_count=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT COUNT(*) FROM tblLoginQueue WHERE world_server_id = $server_choice;" 2>/dev/null)
                    
                    if [ "$server_queue_count" -gt 0 ]; then
                        echo -e "${YELLOW}Delete queue for server $server_choice ($server_queue_count players) - Are you sure? [y/N]: ${NC}"
                        read -n 1 confirm
                        echo
                        if [[ "$confirm" =~ ^[Yy]$ ]]; then
                            execute_sql "DELETE FROM tblLoginQueue WHERE world_server_id = $server_choice;"
                            local delete_result=$?
                            
                            if [ $delete_result -eq 0 ]; then
                                notification="${GREEN}Deleted queue for server $server_choice: removed $server_queue_count players${NC}"
                                trigger_queue_refresh
                            else
                                notification="${RED}Failed to delete queue for server $server_choice${NC}"
                            fi
                        else
                            notification="${BLUE}Queue deletion cancelled${NC}"
                        fi
                    else
                        notification="${YELLOW}Server $server_choice has no queue entries${NC}"
                    fi
                else
                    notification="${YELLOW}Queue deletion cancelled - invalid server choice${NC}"
                fi
                ;;
            "8")
                # Toggle queue mode
                echo
                if [ "$queue_mode" = "1" ]; then
                    echo -n -e "${YELLOW}Change to Chrono mode? [y/N]: ${NC}"
                else
                    echo -n -e "${YELLOW}Change to Dynamic mode? [y/N]: ${NC}"
                fi
                read -n 1 confirm
                echo
                if [[ "$confirm" =~ ^[Yy]$ ]]; then
                    if [ "$queue_mode" = "1" ]; then
                        queue_mode=2
                        notification="${GREEN}Queue mode changed to: (2) Chrono - Time based${NC}"
                    else
                        queue_mode=1
                        notification="${GREEN}Queue mode changed to: (1) Dynamic - Rotate RAID, NORM, GRP, NEWB${NC}"
                    fi
                else
                    notification="${BLUE}Queue mode change cancelled${NC}"
                fi
                ;;
            "p"|"P")
                # Set population offset
                echo
                local current_offset=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT rule_value FROM rule_values WHERE rule_name = 'Quarm:TestPopulationOffset' LIMIT 1;" 2>/dev/null)
                current_offset=${current_offset:-0}
                
                echo -e "${YELLOW}Current population offset: $current_offset${NC}"
                echo -n -e "${YELLOW}Enter new population offset (0-9999): ${NC}"
                read new_offset
                
                if [[ "$new_offset" =~ ^[0-9]+$ ]] && [ "$new_offset" -ge 0 ] && [ "$new_offset" -le 9999 ]; then
                    mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -e "
                        INSERT INTO rule_values (ruleset_id, rule_name, rule_value) 
                        VALUES (1, 'Quarm:TestPopulationOffset', '$new_offset')
                        ON DUPLICATE KEY UPDATE rule_value = '$new_offset';
                    " 2>/dev/null
                    
                    # Verify the update
                    local verified_offset=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT rule_value FROM rule_values WHERE rule_name = 'Quarm:TestPopulationOffset';" 2>/dev/null)
                    
                    notification="${GREEN}Population offset set: $current_offset → $new_offset${NC}"
                    notification+="\n${BLUE}Verified in database: Quarm:TestPopulationOffset = $verified_offset${NC}"
                    notification+="\n${YELLOW}World server will use new value within 5-15 seconds${NC}"
                elif [ -n "$new_offset" ]; then
                    notification="${RED}Invalid input: '$new_offset'. Please enter a number between 0 and 9999.${NC}"
                else
                    notification="${YELLOW}Set population cancelled - no value entered${NC}"
                fi
                ;;
            "q"|"Q")
                echo
                echo -e "${YELLOW}[INFO]${NC} Returning to main menu..."
                restore_main_trap
                tput cnorm
                return 0
                ;;
            "s"|"S")
                # Decrease test offset by 1 (lower population)
                local current_offset=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT rule_value FROM rule_values WHERE rule_name = 'Quarm:TestPopulationOffset' LIMIT 1;" 2>/dev/null)
                current_offset=${current_offset:-0}
                
                if [ "$current_offset" -gt 0 ]; then
                    local new_offset=$((current_offset - 1))
                    mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -e "
                        INSERT INTO rule_values (ruleset_id, rule_name, rule_value) 
                        VALUES (1, 'Quarm:TestPopulationOffset', '$new_offset')
                        ON DUPLICATE KEY UPDATE rule_value = '$new_offset';
                    " 2>/dev/null
                    
                    # Verify the update
                    local verified_offset=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT rule_value FROM rule_values WHERE rule_name = 'Quarm:TestPopulationOffset';" 2>/dev/null)
                    
                    notification="${GREEN}Population offset decreased: $current_offset → $new_offset${NC}"
                    notification+="\n${BLUE}Verified in database: Quarm:TestPopulationOffset = $verified_offset${NC}"
                    notification+="\n${YELLOW}World server will use new value within 5-15 seconds${NC}"
                else
                    notification="${YELLOW}Population offset already at minimum (0)${NC}"
                fi
                ;;
            "w"|"W")
                # Increase test offset by 1 (higher population)
                local current_offset=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT rule_value FROM rule_values WHERE rule_name = 'Quarm:TestPopulationOffset' LIMIT 1;" 2>/dev/null)
                current_offset=${current_offset:-0}
                
                if [ "$current_offset" -lt 9999 ]; then # Reasonable maximum
                    local new_offset=$((current_offset + 1))
                    mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -e "
                        INSERT INTO rule_values (ruleset_id, rule_name, rule_value) 
                        VALUES (1, 'Quarm:TestPopulationOffset', '$new_offset')
                        ON DUPLICATE KEY UPDATE rule_value = '$new_offset';
                    " 2>/dev/null
                    
                    # Verify the update
                    local verified_offset=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT rule_value FROM rule_values WHERE rule_name = 'Quarm:TestPopulationOffset';" 2>/dev/null)
                    
                    notification="${GREEN}Population offset increased: $current_offset → $new_offset${NC}"
                    notification+="\n${BLUE}Verified in database: Quarm:TestPopulationOffset = $verified_offset${NC}"
                    notification+="\n${YELLOW}World server will use new value within 5-15 seconds${NC}"
                else
                    notification="${YELLOW}Population offset already at maximum (9999)${NC}"
                fi
                ;;
            "r"|"R")
                # Toggle raw queue display
                if [ "${show_raw_queue:-false}" = "true" ]; then
                    show_raw_queue="false"
                    notification="${GREEN}Raw queue display: OFF${NC}"
                else
                    show_raw_queue="true"
                    notification="${GREEN}Raw queue display: ON${NC}"
                fi
                ;;
            "x"|"X")
                # Rescan server
                echo
                echo -e "${YELLOW}Rescanning server registration...${NC}"
                
                # Get current server information
                local latest_server=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "
                    SELECT ServerID, ServerLongName, ServerShortName, ServerLastLoginDate
                    FROM tblWorldServerRegistration 
                    ORDER BY ServerID DESC 
                    LIMIT 1;" 2>/dev/null)
                
                if [ -n "$latest_server" ]; then
                    echo "$latest_server" | while IFS=$'\t' read server_id long_name short_name last_login; do
                        echo -e "${GREEN}✓ Current server found:${NC}"
                        echo "   Server ID: $server_id"
                        echo "   Long Name: $long_name"
                        echo "   Short Name: $short_name"  
                        echo "   Last Registration: $last_login"
                    done
                    
                    # Show queue data for this server
                    local queue_entries=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "
                        SELECT COUNT(*) FROM tblLoginQueue WHERE world_server_id = (
                            SELECT ServerID FROM tblWorldServerRegistration ORDER BY ServerID DESC LIMIT 1
                        );" 2>/dev/null)
                    echo "   Queue entries: ${queue_entries:-0}"
                    
                    notification="${GREEN}Server rescan complete - current server ID: $(echo "$latest_server" | cut -f1)${NC}"
                else
                    notification="${RED}Server rescan failed - no server registrations found${NC}"
                fi
                ;;
            # Legacy support for old key mappings - updated mappings
            "f"|"F") key="1"; continue ;;
            "b"|"B") key="2"; continue ;;
            "e"|"E") key="3"; continue ;;
            "d"|"D") key="4"; continue ;;
            "m"|"M") key="8"; continue ;;
            # "r"|"R") key="r"; continue ;;  # Removed - causes infinite loop, 'r' case exists above
            *)
                # No key pressed or other key, continue refresh cycle
                ;;
        esac
    done
}

# Function to restore main trap after live monitor
restore_main_trap() {
    trap 'echo -e "\n${YELLOW}[INFO]${NC} Goodbye!"; tput cnorm; exit 0' INT
}

# Function to advance the queue (simulate someone entering the server)
advance_queue() {
    print_step "Advancing queue (simulating player entry)..."
    
    local world_server_id=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_NAME" -se "SELECT ServerID FROM tblWorldServerRegistration ORDER BY ServerID LIMIT 1;" 2>/dev/null)
    
    if [ -z "$world_server_id" ]; then
        print_error "No world server found in database."
        print_info "Make sure both servers are running."
        return 1
    fi
    
    # Remove the player at position 1
    execute_sql "DELETE FROM tblLoginQueue WHERE world_server_id = $world_server_id AND queue_position = 1;"
    
    # Move everyone else up one position
    execute_sql "UPDATE tblLoginQueue SET queue_position = GREATEST(1, queue_position - 1), estimated_wait = GREATEST(30, (queue_position - 1) * 30), last_updated = NOW() WHERE world_server_id = $world_server_id AND queue_position > 1;"
    
    print_info "Queue advanced - position 1 entered server, all others moved up"
}

# Function to add a test account to the queue
add_test_account_to_queue() {
    print_step "Adding account to queue..."
    
    echo -n "Enter account name: "
    read account_name
    
    if [ -z "$account_name" ]; then
        print_error "Account name cannot be empty"
        return 1
    fi
    
    local world_server_id=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_NAME" -se "SELECT ServerID FROM tblWorldServerRegistration ORDER BY ServerID LIMIT 1;" 2>/dev/null)
    
    if [ -z "$world_server_id" ]; then
        print_error "No world server found in database."
        print_info "Make sure both servers are running."
        return 1
    fi
    
    # Get account ID
    local account_id=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -se "SELECT LoginServerID FROM tblLoginServerAccounts WHERE AccountName = '$account_name';" 2>/dev/null)
    
    if [ -z "$account_id" ]; then
        print_error "Account '$account_name' not found in database."
        return 1
    fi
    
    # Get next available position
    local next_position=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -se "SELECT COALESCE(MAX(queue_position), 0) + 1 FROM tblLoginQueue WHERE world_server_id = $world_server_id;" 2>/dev/null)
    
    # Calculate estimated wait time (60 seconds per position ahead)
    local estimated_wait=$((next_position * 60))
    
    # Add to queue
    execute_sql "INSERT IGNORE INTO tblLoginQueue (account_id, world_server_id, queue_position, estimated_wait, ip_address, queued_timestamp, last_updated) VALUES ($account_id, $world_server_id, $next_position, $estimated_wait, INET_ATON('127.0.0.1'), NOW(), NOW());"
    
    if [ $? -eq 0 ]; then
        print_info "Account '$account_name' added to queue at position $next_position (estimated wait: $estimated_wait seconds)"
    else
        print_error "Failed to add account to queue"
    fi
}

# Function to move account to front of queue (for testing auto-connect)
move_to_front() {
    print_step "Moving account to front of queue..."
    
    echo -n "Enter account name: "
    read account_name
    
    if [ -z "$account_name" ]; then
        print_error "Account name cannot be empty"
        return 1
    fi
    
    local world_server_id=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_NAME" -se "SELECT ServerID FROM tblWorldServerRegistration ORDER BY ServerID LIMIT 1;" 2>/dev/null)
    
    if [ -z "$world_server_id" ]; then
        print_error "No world server found in database."
        print_info "Make sure both servers are running."
        return 1
    fi
    
    # Get account ID
    local account_id=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -se "SELECT LoginServerID FROM tblLoginServerAccounts WHERE AccountName = '$account_name';" 2>/dev/null)
    
    if [ -z "$account_id" ]; then
        print_error "Account '$account_name' not found in database."
        return 1
    fi
    
    # Check if account is in queue
    local current_position=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -se "SELECT queue_position FROM tblLoginQueue WHERE account_id = $account_id AND world_server_id = $world_server_id;" 2>/dev/null)
    
    if [ -z "$current_position" ]; then
        print_error "Account '$account_name' is not in the queue."
        return 1
    fi
    
    # Move everyone at position 1 and above up by 1
    execute_sql "UPDATE tblLoginQueue SET queue_position = queue_position + 1, estimated_wait = queue_position * 60, last_updated = NOW() WHERE world_server_id = $world_server_id AND queue_position >= 1 AND account_id != $account_id;"
    
    # Move target account to position 1
    execute_sql "UPDATE tblLoginQueue SET queue_position = 1, estimated_wait = 30, last_updated = NOW() WHERE account_id = $account_id AND world_server_id = $world_server_id;"
    
    print_info "Account '$account_name' moved to position 1 (ready for auto-connect testing)"
}

# Function to clean up test environment
cleanup() {
    print_step "Cleaning up test environment..."
    
    local world_server_id=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_NAME" -se "SELECT ServerID FROM tblWorldServerRegistration ORDER BY ServerID LIMIT 1;" 2>/dev/null)
    
    if [ -n "$world_server_id" ]; then
        # Clear queue
        execute_sql "DELETE FROM tblLoginQueue WHERE world_server_id = $world_server_id;"
        
        # Reset rules to defaults
        set_rule "Quarm.PlayerPopulationCap" "1200"
        set_rule "Quarm.EnableQueue" "true"
        set_rule "Quarm.TestPopulationOffset" "0"
        set_rule "Quarm.QueueBypassGMLevel" "true"
        set_rule "Quarm.QueueEstimatedWaitPerPlayer" "60"
        set_rule "Quarm.EnableQueueLogging" "true"
        
        # Remove test accounts (optional - comment out if you want to keep them)
        # execute_sql "DELETE FROM tblLoginServerAccounts WHERE AccountName LIKE 'queuetest%';"
    fi
    
    print_info "Test environment cleaned up"
}

# Function to get IP-based population count from server_population table
# Returns the number of active IP reservations tracked by the world server
get_real_player_count() {
    mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT current_population FROM server_population WHERE server_id = 1 LIMIT 1;" 2>/dev/null || echo "0"
}

# Function to get active clients from tblLoginActiveAccounts
get_client_count() {
    # Active clients = people who have logged in and are tracked by login server
    # This is NOT the same as IP reservations - this tracks authentication sessions
    local active_clients=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "
        SELECT COUNT(*) FROM tblLoginActiveAccounts;
    " 2>/dev/null)
    
    echo "${active_clients:-0}"
}

# Function to get detailed account tracking count
get_detailed_account_count() {
    local detailed_count=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "
        SELECT COUNT(*) FROM active_account_connections;
    " 2>/dev/null)
    
    echo "${detailed_count:-0}"
}

# Function to show population comparison
show_population_comparison() {
    print_step "Population Tracking Comparison"
    
    local client_count=$(get_client_count)
    local account_reservations=$(get_real_player_count)
    local detailed_account_count=$(get_detailed_account_count)
    
    echo
    print_info "Population Metrics:"
    print_info "  Account Reservations (Real-time): $account_reservations (account-based tracking)"
    print_info "  Account Reservations (Detailed): $detailed_account_count (with grace period data)"
    echo
    
    # Show differences and what they mean
    if [ "$client_count" -eq "$account_reservations" ]; then
        print_info "✅ Client Count and Account Reservations match - no players in grace periods"
    elif [ "$account_reservations" -gt "$client_count" ]; then
        local grace_players=$((account_reservations - client_count))
        print_info "ℹ️  $grace_players player(s) in grace period (disconnected but reservation held)"
        print_info "   These are players who disconnected but their account reservation is still active"
        print_info "   Grace periods: 60s (normal), 600s (raid members)"
    else
        local missing=$((client_count - account_reservations))
        print_info "⚠️  $missing client(s) not tracked by account system - possible tracking issue"
        print_info "   This may indicate the account tracking system needs investigation"
    fi
    
    # Show detailed vs real-time difference
    if [ "$account_reservations" != "$detailed_account_count" ]; then
        local diff=$((account_reservations - detailed_account_count))
        echo
        if [ "$diff" -gt 0 ]; then
            print_info "📊 Real-time count is $diff higher than detailed count"
            print_info "   Real-time updates every 15s, detailed syncs every 5min"
        else
            print_info "📊 Detailed count is $((-diff)) higher than real-time count"
            print_info "   May indicate recent disconnections not yet synced"
        fi
    fi
}

# Function to debug population tracking
debug_population_tracking() {
    print_step "Debugging Population Tracking System..."
    
    echo
    print_info "1. Checking if server_population table exists..."
    if mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -e "DESCRIBE server_population;" >/dev/null 2>&1; then
        print_info "✓ server_population table exists"
        
        echo
        print_info "2. Table structure:"
        mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" --table -e "DESCRIBE server_population;" 2>/dev/null
        
        echo
        print_info "3. Current table contents:"
        mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" --table -e "SELECT * FROM server_population;" 2>/dev/null
        
        local row_count=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT COUNT(*) FROM server_population;" 2>/dev/null)
        if [ "$row_count" = "0" ]; then
            print_error "Table is empty! This means the world server isn't updating it."
            print_info "Possible causes:"
            print_info "  - World server not running"
            print_info "  - Population tracking code not compiled in"
            print_info "  - Database connection error in world server"
            print_info "  - LoginServer::SendStatus() not running (every 15 seconds)"
        else
            print_info "✓ Table has $row_count row(s)"
            
            local last_updated=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT last_updated FROM server_population WHERE server_id = 1;" 2>/dev/null)
            if [ -n "$last_updated" ]; then
                print_info "✓ Last updated: $last_updated"
                
                # Check how old the last update is
                local seconds_ago=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT TIMESTAMPDIFF(SECOND, last_updated, NOW()) FROM server_population WHERE server_id = 1;" 2>/dev/null)
                if [ "$seconds_ago" -gt 60 ]; then
                    print_error "⚠ Last update was $seconds_ago seconds ago (should update every ~15 seconds)"
                    print_info "World server may not be running or population tracking is broken"
                else
                    print_info "✓ Update is recent ($seconds_ago seconds ago)"
                fi
            fi
        fi
    else
        print_error "✗ server_population table does not exist!"
        print_info "You may need to run database migrations or update your schema."
    fi
    
    echo
    print_info "4. Checking account tracking detail table (active_account_connections)..."
    if mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -e "DESCRIBE active_account_connections;" >/dev/null 2>&1; then
        print_info "✓ active_account_connections table exists"
        
        local account_count=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT COUNT(*) FROM active_account_connections;" 2>/dev/null)
        print_info "✓ Contains $account_count account reservations"
        
        if [ "$account_count" -gt 0 ]; then
            echo
            print_info "Recent account reservations:"
            mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" --table -e "
                SELECT 
                    account_id as 'Account ID',
                    INET_NTOA(ip_address) as 'IP Address',
                    last_seen as 'Last Seen',
                    grace_period as 'Grace (s)',
                    CASE WHEN is_in_raid = 1 THEN 'Yes' ELSE 'No' END as 'Raid'
                FROM active_account_connections 
                ORDER BY last_seen DESC 
                LIMIT 10;
            " 2>/dev/null
        fi
    else
        print_error "✗ active_account_connections table does not exist!"
        print_info "This table stores detailed account reservation data and is required for crash recovery."
    fi
    
    echo
    print_info "5. Testing manual population update..."
    local test_population=999
    if mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -e "INSERT INTO server_population (server_id, current_population) VALUES (1, $test_population) ON DUPLICATE KEY UPDATE current_population = $test_population, last_updated = NOW();" 2>/dev/null; then
        print_info "✓ Manual update successful"
        local new_value=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT current_population FROM server_population WHERE server_id = 1;" 2>/dev/null)
        if [ "$new_value" = "$test_population" ]; then
            print_info "✓ Value updated correctly to $new_value"
            # Reset to 0
            mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -e "UPDATE server_population SET current_population = 0 WHERE server_id = 1;" 2>/dev/null
            print_info "✓ Reset to 0 for normal operation"
        else
            print_error "✗ Value not updated correctly (got: $new_value, expected: $test_population)"
        fi
    else
        print_error "✗ Manual update failed - database permission issue"
    fi
}

# Function to show detailed account tracker status
show_account_tracker_status() {
    print_step "Account Tracker Status and Reservations"
    
    # Get summary counts
    local server_pop=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT COALESCE(current_population, 0) FROM server_population WHERE server_id = 1;" 2>/dev/null)
    local account_detail_count=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT COUNT(*) FROM active_account_connections;" 2>/dev/null)
    
    echo
    print_info "Population Summary:"
    print_info "  Current Population (server_population): ${server_pop:-Unknown}"
    print_info "  Detailed Account Reservations (active_account_connections): ${account_detail_count:-0}"
    
    if [ -n "$server_pop" ] && [ -n "$account_detail_count" ] && [ "$server_pop" != "$account_detail_count" ]; then
        print_info "  Note: Counts may differ - server_population updates every 15s, account details sync every 5min"
    fi
    
    # Show detailed account connections if any exist
    if [ "$account_detail_count" -gt 0 ]; then
        echo
        print_info "Active Account Reservations:"
        mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" --table -e "
            SELECT 
                account_id as 'Account ID',
                INET_NTOA(ip_address) as 'IP Address',
                last_seen as 'Last Seen',
                grace_period as 'Grace Period (s)',
                CASE WHEN is_in_raid = 1 THEN 'Yes' ELSE 'No' END as 'In Raid',
                TIMESTAMPDIFF(SECOND, last_seen, NOW()) as 'Seconds Ago'
            FROM active_account_connections 
            ORDER BY last_seen DESC;
        " 2>/dev/null
        
        # Show grace period statistics
        echo
        print_info "Grace Period Breakdown:"
        mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" --table -e "
            SELECT 
                grace_period as 'Grace Period (s)',
                COUNT(*) as 'Count',
                CASE 
                    WHEN grace_period = 60 THEN 'Normal Players'
                    WHEN grace_period = 600 THEN 'Raid Members'
                    ELSE 'Custom'
                END as 'Type'
            FROM active_account_connections 
            GROUP BY grace_period 
            ORDER BY grace_period;
        " 2>/dev/null
    else
        echo
        print_info "No active account reservations found."
        print_info "This could mean:"
        print_info "  - No players are currently connected"
        print_info "  - World server hasn't synced the detailed table yet (syncs every 5 minutes)"
        print_info "  - Account tracking system is not working properly"
    fi
}

# Function to clear account tracking test data
clear_account_test_data() {
    print_step "Clearing Account Tracking Test Data"
    
    echo
    print_info "This will clear all account reservation data from active_account_connections table."
    print_info "Use this for testing or to reset the account tracking system."
    print_info ""
    echo -n "Are you sure? [y/N]: "
    read confirm
    
    if [[ "$confirm" =~ ^[Yy]$ ]]; then
        local count_before=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT COUNT(*) FROM active_account_connections;" 2>/dev/null)
        
        execute_sql "DELETE FROM active_account_connections;"
        
        if [ $? -eq 0 ]; then
            print_info "✅ Cleared $count_before account reservations from test data"
            print_info "Note: Real player connections will be re-added by the world server"
            trigger_queue_refresh
        else
            print_error "❌ Failed to clear account test data"
        fi
    else
        print_info "Cancelled - no data cleared"
    fi
}

# Function to toggle GM bypass queue
toggle_gm_bypass() {
    print_step "Toggling GM Bypass Queue..."
    
    local current_gm_bypass=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT rule_value FROM rule_values WHERE rule_name = 'Quarm:QueueBypassGMLevel';" 2>/dev/null)
    
    # If rule not found, create it with default value 'true'
    if [ -z "$current_gm_bypass" ]; then
        print_info "GM Bypass rule not found. Creating with default value 'true'..."
        current_gm_bypass="true"
    fi
    
    if [ "$current_gm_bypass" = "true" ]; then
        new_gm_bypass="false"
        print_info "Disabling GM Bypass Queue."
    else
        new_gm_bypass="true"
        print_info "Enabling GM Bypass Queue."
    fi
    
    set_rule "Quarm:QueueBypassGMLevel" "$new_gm_bypass"
    
    print_info "GM Bypass Queue is now: $new_gm_bypass"
}

# Function to toggle queue freeze
toggle_freeze_queue() {
    print_step "Toggling Queue Freeze..."
    
    local current_freeze=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT rule_value FROM rule_values WHERE rule_name = 'Quarm:FreezeQueue';" 2>/dev/null)
    
    # If rule not found, create it with default value 'false'
    if [ -z "$current_freeze" ]; then
        print_info "Freeze Queue rule not found. Creating with default value 'false'..."
        current_freeze="false"
    fi
    
    if [ "$current_freeze" = "true" ]; then
        new_freeze="false"
        print_info "Unfreezing queue - players will now advance in queue normally."
    else
        new_freeze="true"
        print_info "Freezing queue - players will stay at their current positions."
    fi
    
    set_rule "Quarm:FreezeQueue" "$new_freeze"
    
    print_info "Queue freeze is now: $new_freeze"
    if [ "$new_freeze" = "true" ]; then
        print_info "Note: Players can still be added/removed from queue, but positions won't advance."
    else
        print_info "Note: Queue positions will now advance every 30 seconds as normal."
    fi
}

# Function to delete entire queue
delete_entire_queue() {
    print_step "Delete Entire Queue"
    
    echo
    echo -e "${RED}WARNING: This will delete the entire queue for all players!${NC}"
    echo -n -e "${YELLOW}Are you absolutely sure? [y/N]: ${NC}"
    read -n 1 confirm
    echo
    
    if [[ "$confirm" =~ ^[Yy]$ ]]; then
        local world_server_id=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT ServerID FROM tblWorldServerRegistration ORDER BY ServerID LIMIT 1;" 2>/dev/null)
        if [ -n "$world_server_id" ]; then
            print_info "Found world server ID: $world_server_id"
            local queue_count=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "SELECT COUNT(*) FROM tblLoginQueue WHERE world_server_id=$world_server_id;" 2>/dev/null)
            print_info "Current queue has $queue_count players"
            
            execute_sql "DELETE FROM tblLoginQueue WHERE world_server_id=$world_server_id;"
            local delete_result=$?
            
            if [ $delete_result -eq 0 ]; then
                print_info "Cleared queue for world server $world_server_id: removed $queue_count players"
                trigger_queue_refresh
            else
                print_error "Failed to clear queue for world server $world_server_id"
            fi
        else
            print_error "No world server found in database"
            print_error "Make sure your world server is running and registered"
            print_error "Check database connection settings at top of script"
            return 1
        fi
    else
        print_info "Queue deletion cancelled"
        return 0
    fi
}

# Function to clean up orphaned queue entries from old/dead servers
cleanup_orphaned_queue_entries() {
    print_step "Cleaning up orphaned queue entries from old server restarts..."
    
    # Get the current active world server ID
    local current_server_id=$(find_world_server_id)
    
    if [ -z "$current_server_id" ]; then
        print_error "Cannot determine current world server ID - cannot clean up safely"
        return 1
    fi
    
    print_info "Current active server ID: $current_server_id"
    
    # Show orphaned entries before cleanup
    echo
    print_info "Checking for orphaned queue entries..."
    
    local orphaned_count=$(mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" -sN -e "
        SELECT COUNT(*) 
        FROM tblLoginQueue q
        WHERE q.world_server_id != $current_server_id;" 2>/dev/null)
    
    if [ "$orphaned_count" -gt 0 ]; then
        print_info "Found $orphaned_count orphaned queue entries from old server restarts:"
        
        mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" "$DB_NAME" --table -e "
            SELECT 
                q.world_server_id as 'Old Server ID',
                COUNT(*) as 'Orphaned Entries',
                MAX(q.last_updated) as 'Last Updated'
            FROM tblLoginQueue q
            WHERE q.world_server_id != $current_server_id
            GROUP BY q.world_server_id
            ORDER BY q.world_server_id;" 2>/dev/null
        
        echo
        echo -n -e "${YELLOW}Remove these orphaned entries? [y/N]: ${NC}"
        read confirm
        
        if [[ "$confirm" =~ ^[Yy]$ ]]; then
            # Delete orphaned entries
            execute_sql "DELETE FROM tblLoginQueue WHERE world_server_id != $current_server_id;"
            
            if [ $? -eq 0 ]; then
                print_info "✅ Successfully removed $orphaned_count orphaned queue entries"
                print_info "Queue now only contains entries for current server ID: $current_server_id"
                trigger_queue_refresh
            else
                print_error "❌ Failed to remove orphaned entries"
            fi
        else
            print_info "Cleanup cancelled - orphaned entries remain"
        fi
    else
        print_info "✅ No orphaned queue entries found - all entries are for current server ID: $current_server_id"
    fi
}

# Main menu
show_menu() {
    echo
    echo "Queue System Interactive Test Options:"
    echo "1. Live queue monitor"
    echo "2. Set population offset"
    echo "3. Debug population tracking"
    echo "4. Show account tracker status"
    echo "5. Clear account tracking test data"
    echo "6. Delete queue (all players)"
    echo "7. Toggle GM bypass queue"
    echo "8. Toggle freeze queue"
    echo "9. Cleanup orphaned queue entries"
    echo "10. Exit"
    echo
    echo "Press Ctrl+C to exit anytime"
    echo
}

# Main execution
main() {
    print_header
    
    # Set trap for Ctrl+C to exit gracefully
    trap 'echo -e "\n${YELLOW}[INFO]${NC} Goodbye!"; exit 0' INT
    
    while true; do
        show_menu
        echo -n "Choose option [1-10]: "
        read choice
        
        case $choice in
            1)
                live_queue_monitor
                ;;
            2)
                set_population_offset
                ;;
            3)
                debug_population_tracking
                ;;
            4)
                show_account_tracker_status
                ;;
            5)
                clear_account_test_data
                ;;
            6)
                delete_entire_queue
                ;;
            7)
                toggle_gm_bypass
                ;;
            8)
                toggle_freeze_queue
                ;;
            9)
                cleanup_orphaned_queue_entries
                ;;
            10)
                print_info "Goodbye!"
                exit 0
                ;;
            *)
                print_error "Invalid option. Please choose 1-10."
                ;;
        esac
    done
}

# Check if database is accessible
if ! mysql -h"$DB_HOST" -u"$DB_USER" -p"$DB_PASS" -e "USE $DB_NAME;" 2>/dev/null; then
    print_error "Cannot connect to database. Please check connection details."
    print_info "Edit this script to update DB_HOST, DB_USER, DB_PASS, DB_NAME variables"
    exit 1
fi

# Show startup configuration
print_header
print_info "Configuration loaded:"
print_info "  Config file: $CONFIG_PATH"
print_info "  Database: $DB_HOST/$DB_NAME (user: $DB_USER)"
print_info "  Server names: '$WORLD_SERVER_NAME' / '$WORLD_SERVER_SHORT_NAME'"
print_info "  Auto-detect server: $AUTO_DETECT_SERVER"
echo

# Run main menu
main

