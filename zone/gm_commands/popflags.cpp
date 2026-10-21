#include "../client.h"

#include <cstdlib>
#include <list>
#include <string>
#include <unordered_map>

using PopFlagMap = std::unordered_map<std::string, std::string>;

static std::string PopFlagsGet(
        const PopFlagMap& flags,
        const char* name
)
{
        const auto flag = flags.find(name);

        if (flag == flags.end()) {
                return {};
        }

        return flag->second;
}

static bool PopFlagsHas(
        const PopFlagMap& flags,
        const char* name
)
{
        return flags.find(name) != flags.end();
}

static bool PopFlagsBit(
        const std::string& value,
        size_t position
)
{
        return value.size() > position && value[position] == '1';
}

static long PopFlagsStage(
        const PopFlagMap& flags,
        const char* name
)
{
        const auto value = PopFlagsGet(flags, name);

        if (value.empty()) {
                return 0;
        }

        char* end = nullptr;
        const long stage = std::strtol(value.c_str(), &end, 10);

        if (!end || *end != '\0' || stage < 0) {
                return 0;
        }

        return stage;
}

static void PopFlagsPrintStage(
        Client* c,
        const char* label,
        const std::string& value,
        const char* const descriptions[],
        size_t description_count
)
{
        if (value.empty()) {
                c->Message(Chat::White, "%s: Not started", label);
                return;
        }

        char* end = nullptr;
        const long stage = std::strtol(value.c_str(), &end, 10);

        if (
                !end ||
                *end != '\0' ||
                stage < 1 ||
                static_cast<size_t>(stage) > description_count
        ) {
                c->Message(Chat::White, "%s: Progress recorded", label);
                return;
        }

        c->Message(
                Chat::White,
                "%s: %s",
                label,
                descriptions[stage - 1]
        );
}

static bool PopFlagsHasPendingMemories(
        const PopFlagMap& flags
)
{
        static const char* checklist_flags[] = {
                "cl_grummus",
                "cl_maze",
                "cl_behemoth",
                "cl_aerindar",
                "cl_terris",
                "cl_bertox",
                "cl_keeper",
                "cl_saryrn",
                "cl_vallon",
                "cl_tallon",
                "cl_rallos",
                "cl_karana",
                "cl_mmarr",
                "cl_mmarr_book",
                "cl_solusek"
        };

        for (const auto* flag_name : checklist_flags) {
                if (PopFlagsHas(flags, flag_name)) {
                        return true;
                }
        }

        return false;
}

static bool PopFlagsHasReadyMemory(
        const PopFlagMap& flags
)
{
        const auto zeks = PopFlagsStage(flags, "zeks");

        return
                (PopFlagsHas(flags, "cl_grummus") && PopFlagsStage(flags, "fuirstel") == 1) ||
                (PopFlagsHas(flags, "cl_maze") && PopFlagsStage(flags, "thelin") == 1) ||
                (PopFlagsHas(flags, "cl_behemoth") && zeks == 1) ||
                (PopFlagsHas(flags, "cl_aerindar") && PopFlagsStage(flags, "mavuin") == 3) ||
                (PopFlagsHas(flags, "cl_terris") && PopFlagsStage(flags, "thelin") == 2) ||
                (PopFlagsHas(flags, "cl_bertox") && PopFlagsStage(flags, "fuirstel") == 3) ||
                (PopFlagsHas(flags, "cl_keeper") && PopFlagsStage(flags, "tylis") == 1) ||
                (PopFlagsHas(flags, "cl_saryrn") && PopFlagsStage(flags, "tylis") == 2) ||
                (PopFlagsHas(flags, "cl_vallon") && (zeks == 2 || zeks == 4)) ||
                (PopFlagsHas(flags, "cl_tallon") && (zeks == 2 || zeks == 3)) ||
                (PopFlagsHas(flags, "cl_rallos") && zeks == 6) ||
                (PopFlagsHas(flags, "cl_karana") && PopFlagsStage(flags, "karana") == 3) ||
                (PopFlagsHas(flags, "cl_mmarr") && PopFlagsGet(flags, "hohtrials") == "111") ||
                (PopFlagsHas(flags, "cl_mmarr_book") && PopFlagsGet(flags, "hohtrials") == "111") ||
                (PopFlagsHas(flags, "cl_solusek") && zeks == 7);
}

static void PopFlagsPrintPending(
        Client* c,
        const PopFlagMap& flags,
        const char* flag_name,
        const char* description
)
{
        if (PopFlagsHas(flags, "time")) {
                return;
        }

        if (PopFlagsHas(flags, flag_name)) {
                c->Message(
                        Chat::Yellow,
                        "Pending memory: %s",
                        description
                );
        }
}

// A completed or combined flag takes precedence over a stale checklist memory.
static void PopFlagsPrintBoss(Client* c, const PopFlagMap& flags,
        const char* label, bool complete, const char* pending)
{
        c->Message(Chat::White, "%s: %s", label,
                (complete || PopFlagsHas(flags, "time")) ? "Complete" :
                PopFlagsHas(flags, pending) ? "Pending memory" : "Missing");
}

static void PopFlagsPrintSeerNotice(
        Client* c,
        const PopFlagMap& flags
)
{
        if (PopFlagsHas(flags, "time") || !PopFlagsHasPendingMemories(flags)) {
                return;
        }

        if (PopFlagsHasReadyMemory(flags)) {
                c->Message(
                        Chat::Yellow,
                        "A checklist memory is ready to be unlocked."
                );

                c->Message(
                        Chat::Yellow,
                        "Sit near Seer Mal Nae`Shi and say 'unlock memories', then check #popflags again."
                );
        }
        else {
                c->Message(
                        Chat::Yellow,
                        "Pending checklist memories exist, but their prerequisite steps are incomplete."
                );

                c->Message(
                        Chat::Yellow,
                        "Complete the unfinished progression shown above, then return to Seer Mal Nae`Shi."
                );
        }
}

static const char* PopFlagsProgressStatus(
        bool started,
        bool complete
)
{
        if (complete) {
                return "Complete";
        }

        if (started) {
                return "In progress";
        }

        return "Not started";
}

static bool PopFlagsTier1Complete(const PopFlagMap& flags)
{
        return PopFlagsHas(flags, "time") || (
                PopFlagsStage(flags, "mavuin") >= 3 &&
                PopFlagsStage(flags, "fuirstel") >= 5 &&
                PopFlagsStage(flags, "thelin") >= 4 &&
                PopFlagsHas(flags, "poi_door") &&
                PopFlagsStage(flags, "zeks") >= 2
        );
}

static bool PopFlagsTier2Complete(const PopFlagMap& flags)
{
        return PopFlagsHas(flags, "time") || (
                PopFlagsStage(flags, "aerindar") >= 2 &&
                (PopFlagsStage(flags, "karana") >= 2 ||
                        PopFlagsHas(flags, "zebuxoruk")) &&
                PopFlagsHas(flags, "bertox_key") &&
                PopFlagsStage(flags, "tylis") >= 2 &&
                (PopFlagsHas(flags, "saryrn") || PopFlagsHas(flags, "cipher"))
        );
}

static bool PopFlagsTier3Complete(const PopFlagMap& flags)
{
        const auto hohtrials = PopFlagsGet(flags, "hohtrials");
        const auto sol_room = PopFlagsGet(flags, "sol_room");

        return PopFlagsHas(flags, "time") || (
                PopFlagsBit(hohtrials, 0) && PopFlagsBit(hohtrials, 1) &&
                PopFlagsBit(hohtrials, 2) && PopFlagsHas(flags, "cipher") &&
                PopFlagsStage(flags, "zebuxoruk") >= 2 &&
                PopFlagsStage(flags, "zeks") >= 7 &&
                PopFlagsBit(sol_room, 0) && PopFlagsBit(sol_room, 1) &&
                PopFlagsBit(sol_room, 2) && PopFlagsBit(sol_room, 3) &&
                PopFlagsBit(sol_room, 4) && PopFlagsStage(flags, "pofire") >= 2
        );
}

static bool PopFlagsTier4Complete(const PopFlagMap& flags)
{
        return PopFlagsHas(flags, "time");
}

static void PopFlagsPrintTierCompletion(Client* c, bool complete, const char* lore)
{
        if (complete) {
                c->Message(Chat::Lime, "Tier complete: %s", lore);
        }
}

static void PopFlagsPrintOverview(
        Client* c,
        const PopFlagMap& flags
)
{
        const bool tier1_started =
                PopFlagsHas(flags, "mavuin") ||
                PopFlagsHas(flags, "seventh") ||
                PopFlagsHas(flags, "fuirstel") ||
                PopFlagsHas(flags, "grummus") ||
                PopFlagsHas(flags, "thelin") ||
                PopFlagsHas(flags, "poi_door") ||
                PopFlagsHas(flags, "cl_behemoth");

        const bool tier2_started =
                PopFlagsHas(flags, "aerindar") ||
                PopFlagsHas(flags, "karana") ||
                PopFlagsHas(flags, "bertox_key") ||
                PopFlagsHas(flags, "tylis") ||
                PopFlagsHas(flags, "saryrn");

        const bool tier3_started =
                PopFlagsHas(flags, "hohtrials") ||
                PopFlagsHas(flags, "mmarr") ||
                PopFlagsHas(flags, "mmarr_book") ||
                PopFlagsHas(flags, "cipher") ||
                PopFlagsHas(flags, "zebuxoruk") ||
                PopFlagsHas(flags, "zeks") ||
                PopFlagsHas(flags, "sol_room") ||
                PopFlagsHas(flags, "pofire");

        const bool tier4_started =
                PopFlagsHas(flags, "earthb_key") ||
                PopFlagsHas(flags, "time");

        const bool tier1_complete = PopFlagsTier1Complete(flags);
        const bool tier2_complete = PopFlagsTier2Complete(flags);
        const bool tier3_complete = PopFlagsTier3Complete(flags);
        const bool tier4_complete = PopFlagsTier4Complete(flags);

        c->Message(
                Chat::Lime,
                "=== Planes of Power Progression ==="
        );

        c->Message(
                Chat::White,
                "Tier 1: %s",
                PopFlagsProgressStatus(tier1_started, tier1_complete)
        );

        c->Message(
                Chat::White,
                "Tier 2: %s",
                PopFlagsProgressStatus(tier2_started, tier2_complete)
        );

        c->Message(
                Chat::White,
                "Tier 3: %s",
                PopFlagsProgressStatus(tier3_started, tier3_complete)
        );

        c->Message(
                Chat::White,
                "Tier 4: %s",
                PopFlagsProgressStatus(
                        tier4_started,
                        tier4_complete
                )
        );

        c->Message(
                Chat::White,
                "Tier 5 - Plane of Time: %s",
                PopFlagsProgressStatus(PopFlagsHas(flags, "time"), PopFlagsHas(flags, "time"))
        );

        PopFlagsPrintSeerNotice(c, flags);

        c->Message(
                Chat::Yellow,
                "Details: #popflags 1, 2, 3, 4, or 5 (tier1-tier5 also work)."
        );
}

static void PopFlagsPrintTier1(
        Client* c,
        const PopFlagMap& flags
)
{
        static const char* mavuin_stages[] = {
                "The evidence needed to save Mavuin has been requested",
                "The Tribunal has agreed to hear Mavuin's case",
                "Mavuin's case is complete"
        };

        static const char* fuirstel_stages[] = {
                "Obtain the Ward for Milyk",
                "Grummus defeated; return to Elder Fuirstel",
                "Elder Fuirstel aided; defeat Bertoxxulous",
                "Bertoxxulous defeated; return to Elder Fuirstel",
                "Fuirstel progression complete"
        };

        static const char* thelin_stages[] = {
                "Help Thelin escape the hedge maze",
                "Defeat Terris Thule",
                "Terris Thule was defeated",
                "Thelin was released from Terris Thule",
                "Nightmare progression complete"
        };

        c->Message(Chat::Lime, "=== Tier 1 Progression ===");

        c->Message(Chat::Lime, "--- Plane of Justice ---");

        PopFlagsPrintStage(
                c,
                "Mavuin's case",
                PopFlagsGet(flags, "mavuin"),
                mavuin_stages,
                3
        );

        c->Message(
                Chat::White,
                "Seventh Hammer access: %s",
                PopFlagsHas(flags, "seventh") ?
                        "Unlocked" :
                        "Locked"
        );

        c->Message(Chat::Lime, "--- Plane of Disease ---");

        PopFlagsPrintStage(
                c,
                "Fuirstel progression",
                PopFlagsGet(flags, "fuirstel"),
                fuirstel_stages,
                5
        );

        c->Message(
                Chat::White,
                "Crypt of Decay access: %s",
                PopFlagsHas(flags, "grummus") ?
                        "Unlocked" :
                        "Locked"
        );

        PopFlagsPrintBoss(c, flags, "Grummus flag",
                PopFlagsStage(flags, "fuirstel") >= 2, "cl_grummus");

        c->Message(Chat::Lime, "--- Plane of Nightmare ---");

        PopFlagsPrintStage(
                c,
                "Thelin progression",
                PopFlagsGet(flags, "thelin"),
                thelin_stages,
                5
        );

        PopFlagsPrintPending(c, flags, "cl_maze", "Thelin's hedge maze");
        PopFlagsPrintBoss(c, flags, "Terris Thule flag",
                PopFlagsStage(flags, "thelin") >= 3, "cl_terris");

        c->Message(Chat::Lime, "--- Plane of Innovation ---");

        c->Message(
                Chat::White,
                "Factory door access: %s",
                PopFlagsHas(flags, "poi_door") ?
                        "Unlocked" :
                        "Locked"
        );

        PopFlagsPrintBoss(c, flags, "Manaetic Behemoth flag",
                PopFlagsStage(flags, "zeks") >= 2, "cl_behemoth");

        PopFlagsPrintSeerNotice(c, flags);
        PopFlagsPrintTierCompletion(
                c,
                PopFlagsTier1Complete(flags),
                "Justice has been served, Disease's grip weakened, Nightmare ended, and Innovation's hidden plot uncovered."
        );
}

static void PopFlagsPrintTier2(
        Client* c,
        const PopFlagMap& flags
)
{
        static const char* aerindar_stages[] = {
                "Aerin`Dar defeated; the meaning of Justice remains",
                "Halls of Honor access unlocked"
        };

        static const char* karana_stages[] = {
                "Prove yourself to Askr",
                "Complete Mavuin's case, then use the Storms shrine to enter Bastion of Thunder",
                "Bastion of Thunder progression recorded",
                "Karana's information obtained"
        };

        static const char* tylis_stages[] = {
                "Rescue Tylis from the Plane of Torment",
                "Tylis progression complete"
        };

        c->Message(Chat::Lime, "=== Tier 2 Progression ===");

        c->Message(Chat::Lime, "--- Plane of Valor ---");

        PopFlagsPrintStage(
                c,
                "Aerin`Dar progression",
                PopFlagsGet(flags, "aerindar"),
                aerindar_stages,
                2
        );

        PopFlagsPrintBoss(c, flags, "Aerin`Dar flag",
                PopFlagsStage(flags, "aerindar") >= 1, "cl_aerindar");

        c->Message(Chat::Lime, "--- Plane of Storms ---");

        if (PopFlagsHas(flags, "zebuxoruk")) {
                c->Message(
                        Chat::White,
                        "Askr and Karana progression: Complete; combined into Zebuxoruk lore"
                );
        }
        else {
                PopFlagsPrintStage(
                        c,
                        "Askr and Karana progression",
                        PopFlagsGet(flags, "karana"),
                        karana_stages,
                        4
                );
        }

        PopFlagsPrintPending(c, flags, "cl_karana", "Karana");

        c->Message(Chat::Lime, "--- Crypt of Decay ---");

        c->Message(
                Chat::White,
                "Crypt of Decay access: %s",
                PopFlagsHas(flags, "grummus") ?
                        "Unlocked" :
                        "Locked"
        );

        c->Message(
                Chat::White,
                "Lower Crypt access: %s",
                PopFlagsHas(flags, "bertox_key") ?
                        "Unlocked" :
                        "Locked"
        );

        PopFlagsPrintBoss(c, flags, "Bertoxxulous flag",
                PopFlagsStage(flags, "fuirstel") >= 4, "cl_bertox");

        c->Message(Chat::Lime, "--- Plane of Torment ---");

        PopFlagsPrintStage(
                c,
                "Tylis progression",
                PopFlagsGet(flags, "tylis"),
                tylis_stages,
                2
        );

        PopFlagsPrintBoss(c, flags, "Keeper of Sorrows flag",
                PopFlagsStage(flags, "tylis") >= 2, "cl_keeper");

        PopFlagsPrintBoss(c, flags, "Saryrn cipher half",
                PopFlagsHas(flags, "saryrn") || PopFlagsHas(flags, "cipher"), "cl_saryrn");

        PopFlagsPrintSeerNotice(c, flags);
        PopFlagsPrintTierCompletion(
                c,
                PopFlagsTier2Complete(flags),
                "Valor has been proven, Storms weathered, the Crypt of Decay purged, and Torment overcome."
        );
}

static void PopFlagsPrintTier3(
        Client* c,
        const PopFlagMap& flags
)
{
        static const char* zek_stages[] = {
                "Initial Zek progression recorded",
                "Meet Giwin in Drunder",
                "Vallon Zek's information obtained",
                "Tallon Zek's information obtained",
                "Both Zek information sets obtained",
                "Defeat Rallos Zek",
                "Zek progression complete"
        };

        static const char* karana_stages[] = {
                "Return to Askr",
                "Complete Mavuin's case, then use the Storms shrine to enter Bastion of Thunder",
                "Karana progression continues",
                "Karana's information obtained"
        };

        const auto hohtrials = PopFlagsGet(flags, "hohtrials");
        const auto sol_room = PopFlagsGet(flags, "sol_room");

        c->Message(Chat::Lime, "=== Tier 3 Progression ===");

        c->Message(Chat::Lime, "--- Halls of Honor ---");

        if (hohtrials.empty()) {
                c->Message(
                        Chat::White,
                        "Halls of Honor trials: None completed"
                );
        }
        else {
                c->Message(
                        Chat::White,
                        "Rydda`Dar trial: %s",
                        PopFlagsBit(hohtrials, 0) ?
                                "Complete" :
                                "Incomplete"
                );

                c->Message(
                        Chat::White,
                        "Village trial: %s",
                        PopFlagsBit(hohtrials, 1) ?
                                "Complete" :
                                "Incomplete"
                );

                c->Message(
                        Chat::White,
                        "Nomad trial: %s",
                        PopFlagsBit(hohtrials, 2) ?
                                "Complete" :
                                "Incomplete"
                );
        }

        PopFlagsPrintBoss(c, flags, "Mithaniel Marr cipher half",
                PopFlagsHas(flags, "mmarr") || PopFlagsHas(flags, "cipher"), "cl_mmarr");

        PopFlagsPrintBoss(c, flags, "Mithaniel Marr book",
                PopFlagsHas(flags, "mmarr_book") || PopFlagsHas(flags, "zebuxoruk"), "cl_mmarr_book");

        c->Message(Chat::Lime, "--- Bastion of Thunder ---");

        if (PopFlagsHas(flags, "zebuxoruk")) {
                c->Message(
                        Chat::White,
                        "Agnarr and Karana progression: Complete; combined into Zebuxoruk lore"
                );
        }
        else {
                PopFlagsPrintStage(
                        c,
                        "Agnarr and Karana progression",
                        PopFlagsGet(flags, "karana"),
                        karana_stages,
                        4
                );
        }

        PopFlagsPrintBoss(c, flags, "Agnarr / Karana flag",
                PopFlagsStage(flags, "karana") >= 4 || PopFlagsHas(flags, "zebuxoruk"), "cl_karana");

        c->Message(Chat::Lime, "--- Plane of Tactics ---");

        PopFlagsPrintStage(
                c,
                "Giwin and Zek progression",
                PopFlagsGet(flags, "zeks"),
                zek_stages,
                7
        );

        const auto zeks = PopFlagsStage(flags, "zeks");
        PopFlagsPrintBoss(c, flags, "Vallon Zek flag",
                zeks == 3 || zeks >= 5, "cl_vallon");
        PopFlagsPrintBoss(c, flags, "Tallon Zek flag",
                zeks == 4 || zeks >= 5, "cl_tallon");
        PopFlagsPrintBoss(c, flags, "Rallos Zek flag",
                zeks >= 7, "cl_rallos");

        c->Message(Chat::Lime, "--- Grand Librarian Maelin ---");

        c->Message(
                Chat::White,
                "Cipher information: %s",
                PopFlagsHas(flags, "cipher") ?
                        "Received" :
                        "Missing"
        );

        c->Message(
                Chat::White,
                "Zebuxoruk lore: %s",
                !PopFlagsGet(flags, "zebuxoruk").empty() ?
                        "Received" :
                        "Missing"
        );

        c->Message(
                Chat::White,
                "Combined Zek information: %s",
                PopFlagsStage(flags, "zeks") >= 5 ?
                        "Received" :
                        "Missing"
        );

        c->Message(
                Chat::White,
                "Final elemental information: %s",
                PopFlagsStage(flags, "zebuxoruk") >= 2 ?
                        "Received" :
                        "Missing"
        );

        c->Message(
                Chat::Yellow,
                "If one of these is missing, hail Maelin and ask about new lore and new information."
        );

        c->Message(Chat::Lime, "--- Tower of Solusek Ro ---");

        const bool miak_complete = PopFlagsStage(flags, "pofire") >= 1 || PopFlagsHas(flags, "time");
        c->Message(Chat::White, "Miak the Searedsoul preflag: %s", miak_complete ? "Complete" : "Missing");
        if (!miak_complete) {
                c->Message(Chat::Yellow, "Speak to Miak the Searedsoul in Tranquility about 'portal's destination'; hailing alone does not grant the preflag.");
        }

        if (sol_room.empty()) {
                c->Message(
                        Chat::White,
                        "Tower wing flags: None completed"
                );
        }
        else {
                c->Message(
                        Chat::White,
                        "Xuzl: %s",
                        PopFlagsBit(sol_room, 0) ?
                                "Complete" :
                                "Incomplete"
                );

                c->Message(
                        Chat::White,
                        "Arlyxir: %s",
                        PopFlagsBit(sol_room, 1) ?
                                "Complete" :
                                "Incomplete"
                );

                c->Message(
                        Chat::White,
                        "Dresolik: %s",
                        PopFlagsBit(sol_room, 2) ?
                                "Complete" :
                                "Incomplete"
                );

                c->Message(
                        Chat::White,
                        "Rizlona: %s",
                        PopFlagsBit(sol_room, 3) ?
                                "Complete" :
                                "Incomplete"
                );

                c->Message(
                        Chat::White,
                        "Jiva: %s",
                        PopFlagsBit(sol_room, 4) ?
                                "Complete" :
                                "Incomplete"
                );
        }

        c->Message(
                Chat::White,
                "Plane of Fire progression: %s",
                PopFlagsStage(flags, "pofire") >= 2 ?
                        "Unlocked" :
                        PopFlagsHas(flags, "pofire") ?
                                "In progress" :
                                "Not started"
        );

        PopFlagsPrintBoss(c, flags, "Solusek Ro flag",
                PopFlagsStage(flags, "pofire") >= 2, "cl_solusek");

        PopFlagsPrintSeerNotice(c, flags);
        PopFlagsPrintTierCompletion(
                c,
                PopFlagsTier3Complete(flags),
                "Honor has been earned, the Bastion of Thunder conquered, the Warlord defeated, and the Burning Prince's plot exposed. The Elemental Planes now stand open."
        );
}

static void PopFlagsPrintTier4(
        Client* c,
        const PopFlagMap& flags
)
{
        c->Message(Chat::Lime, "=== Tier 4 Progression ===");

        c->Message(Chat::Lime, "--- Elemental Planes ---");

        c->Message(
                Chat::White,
                "Air, Earth, and Water access: %s",
                PopFlagsStage(flags, "zebuxoruk") >= 2 ?
                        "Unlocked" :
                        "Locked"
        );

        c->Message(
                Chat::White,
                "Plane of Fire access: %s",
                PopFlagsStage(flags, "pofire") >= 2 ?
                        "Unlocked" :
                        "Locked"
        );

        c->Message(
                Chat::White,
                "Plane of Earth B access: %s",
                PopFlagsHas(flags, "earthb_key") ?
                        "Unlocked" :
                        "Locked"
        );

        c->Message(
                Chat::White,
                "Plane of Time access: %s",
                PopFlagsHas(flags, "time") ?
                        "Unlocked" :
                        "Locked"
        );

        PopFlagsPrintSeerNotice(c, flags);
        PopFlagsPrintTierCompletion(
                c,
                PopFlagsTier4Complete(flags),
                "The powers of Air, Earth, Fire, and Water have been joined. The way into the Plane of Time now stands open."
        );
}

static void PopFlagsPrintTime(
        Client* c,
        const PopFlagMap& flags
)
{
        c->Message(Chat::Lime, "=== Plane of Time ===");

        c->Message(
                Chat::White,
                "Plane of Time access: %s",
                PopFlagsHas(flags, "time") ?
                        "Unlocked" :
                        "Locked"
        );

        if (!PopFlagsHas(flags, "time")) {
                c->Message(
                        Chat::Yellow,
                        "Complete the elemental progression, combine the four elemental essences, and return to Grand Librarian Maelin."
                );
        }

        PopFlagsPrintSeerNotice(c, flags);
        PopFlagsPrintTierCompletion(
                c,
                PopFlagsHas(flags, "time"),
                "The Plane of Time recognizes your soul. Beyond its shifting portals, the gods await."
        );
}

static void PopFlagsPrintAll(
        Client* c,
        const PopFlagMap& flags
)
{
        PopFlagsPrintOverview(c, flags);
        PopFlagsPrintTier1(c, flags);
        PopFlagsPrintTier2(c, flags);
        PopFlagsPrintTier3(c, flags);
        PopFlagsPrintTier4(c, flags);
        PopFlagsPrintTime(c, flags);
}

void command_popflags(Client* c, const Seperator* sep)
{
        if (!c) {
                return;
        }

        PopFlagMap flags;
        std::list<QGlobal> character_globals;

        QGlobalCache::GetQGlobals(
                character_globals,
                nullptr,
                c,
                zone
        );

        for (const auto& global : character_globals) {
                flags[global.name] = global.value;
        }

        if (!sep->arg[1][0] || !strcasecmp(sep->arg[1], "overview")) {
                PopFlagsPrintOverview(c, flags);
        }
        else if (
                !strcasecmp(sep->arg[1], "1") ||
                !strcasecmp(sep->arg[1], "tier1") ||
                !strcasecmp(sep->arg[1], "t1")
        ) {
                PopFlagsPrintTier1(c, flags);
        }
        else if (
                !strcasecmp(sep->arg[1], "2") ||
                !strcasecmp(sep->arg[1], "tier2") ||
                !strcasecmp(sep->arg[1], "t2")
        ) {
                PopFlagsPrintTier2(c, flags);
        }
        else if (
                !strcasecmp(sep->arg[1], "3") ||
                !strcasecmp(sep->arg[1], "tier3") ||
                !strcasecmp(sep->arg[1], "t3")
        ) {
                PopFlagsPrintTier3(c, flags);
        }
        else if (
                !strcasecmp(sep->arg[1], "4") ||
                !strcasecmp(sep->arg[1], "tier4") ||
                !strcasecmp(sep->arg[1], "t4")
        ) {
                PopFlagsPrintTier4(c, flags);
        }
        else if (
                !strcasecmp(sep->arg[1], "5") ||
                !strcasecmp(sep->arg[1], "tier5") ||
                !strcasecmp(sep->arg[1], "t5") ||
                !strcasecmp(sep->arg[1], "time") ||
                !strcasecmp(sep->arg[1], "potime")
        ) {
                PopFlagsPrintTime(c, flags);
        }
        else if (!strcasecmp(sep->arg[1], "all")) {
                if (c->Admin() < 80) {
                        c->Message(
                                Chat::Red,
                                "The all option is restricted to server staff."
                        );
                        return;
                }

                PopFlagsPrintAll(c, flags);
        }
        else {
                c->Message(
                        Chat::Red,
                        "Unknown #popflags section: %s",
                        sep->arg[1]
                );

                c->Message(
                        Chat::White,
                        "Valid sections: overview, 1-5, tier1-tier5, and time."
                );
        }
}
