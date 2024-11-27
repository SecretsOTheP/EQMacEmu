/**
 * DO NOT MODIFY THIS FILE
 *
 * This repository was automatically generated and is NOT to be modified directly.
 * Any repository modifications are meant to be made to the repository extending the base.
 * Any modifications to base repositories are to be made by the generator only
 *
 * @generator ./utils/scripts/generators/repository-generator.pl
 * @docs https://eqemu.gitbook.io/server/in-development/developer-area/repositories
 */

#ifndef EQEMU_BASE_HARDCORE_GHOST_INVENTORY_REPOSITORY_H
#define EQEMU_BASE_HARDCORE_GHOST_INVENTORY_REPOSITORY_H

#include "../../database.h"
#include "../../strings.h"
#include <ctime>

class BaseHardcoreGhostInventoryRepository {
public:
    struct HardcoreGhostInventory {
        int32_t     ghost_id;
        uint32_t    slotid;
        uint32_t    itemid;
        uint16_t    charges;
        std::string custom_data;
        int32_t     serialnumber;
        int8_t      initialserial;
    };

    static std::string PrimaryKey()
    {
        return std::string("ghost_id");
    }

    static std::vector<std::string> Columns()
    {
        return {
            "ghost_id",
            "slotid",
            "itemid",
            "charges",
            "custom_data",
            "serialnumber",
            "initialserial",
        };
    }

    static std::vector<std::string> SelectColumns()
    {
        return {
            "ghost_id",
            "slotid",
            "itemid",
            "charges",
            "custom_data",
            "serialnumber",
            "initialserial",
        };
    }

    static std::string ColumnsRaw()
    {
        return std::string(Strings::Implode(", ", Columns()));
    }

    static std::string SelectColumnsRaw()
    {
        return std::string(Strings::Implode(", ", SelectColumns()));
    }

    static std::string TableName()
    {
        return std::string("hardcore_ghosts_inventory");
    }

    static std::string BaseSelect()
    {
        return fmt::format(
            "SELECT {} FROM {}",
            SelectColumnsRaw(),
            TableName()
        );
    }

    static std::string BaseInsert()
    {
        return fmt::format(
            "INSERT INTO {} ({}) ",
            TableName(),
            ColumnsRaw()
        );
    }

    static HardcoreGhostInventory NewEntity()
    {
        HardcoreGhostInventory e{};

        e.ghost_id      = 0;
        e.slotid        = 0;
        e.itemid        = 0;
        e.charges       = 0;
        e.custom_data   = "";
        e.serialnumber  = 0;
        e.initialserial = 0;

        return e;
    }

    static HardcoreGhostInventory GetHardcoreGhostInventory(
        const std::vector<HardcoreGhostInventory> &hardcore_ghost_inventories,
        int hardcore_ghost_inventory_id
    )
    {
        for (auto &hardcore_ghost_inventory : hardcore_ghost_inventories) {
            if (hardcore_ghost_inventory.ghost_id == hardcore_ghost_inventory_id) {
                return hardcore_ghost_inventory;
            }
        }

        return NewEntity();
    }

    static HardcoreGhostInventory FindOne(
        Database& db,
        int hardcore_ghost_inventory_id
    )
    {
        auto results = db.QueryDatabase(
            fmt::format(
                "{} WHERE {} = {} LIMIT 1",
                BaseSelect(),
                PrimaryKey(),
                hardcore_ghost_inventory_id
            )
        );

        auto row = results.begin();
        if (results.RowCount() == 1) {
            HardcoreGhostInventory e{};

            e.ghost_id      = row[0] ? static_cast<int32_t>(atoi(row[0])) : 0;
            e.slotid        = row[1] ? static_cast<uint32_t>(strtoul(row[1], nullptr, 10)) : 0;
            e.itemid        = row[2] ? static_cast<uint32_t>(strtoul(row[2], nullptr, 10)) : 0;
            e.charges       = row[3] ? static_cast<uint16_t>(strtoul(row[3], nullptr, 10)) : 0;
            e.custom_data   = row[4] ? row[4] : "";
            e.serialnumber  = row[5] ? static_cast<int32_t>(atoi(row[5])) : 0;
            e.initialserial = row[6] ? static_cast<int8_t>(atoi(row[6])) : 0;

            return e;
        }

        return NewEntity();
    }

    static int DeleteOne(
        Database& db,
        int hardcore_ghost_inventory_id
    )
    {
        auto results = db.QueryDatabase(
            fmt::format(
                "DELETE FROM {} WHERE {} = {}",
                TableName(),
                PrimaryKey(),
                hardcore_ghost_inventory_id
            )
        );

        return (results.Success() ? results.RowsAffected() : 0);
    }

    static int UpdateOne(
        Database& db,
        const HardcoreGhostInventory &e
    )
    {
        std::vector<std::string> v;

        auto columns = Columns();

        v.push_back(columns[0] + " = " + std::to_string(e.ghost_id));
        v.push_back(columns[1] + " = " + std::to_string(e.slotid));
        v.push_back(columns[2] + " = " + std::to_string(e.itemid));
        v.push_back(columns[3] + " = " + std::to_string(e.charges));
        v.push_back(columns[4] + " = '" + Strings::Escape(e.custom_data) + "'");
        v.push_back(columns[5] + " = " + std::to_string(e.serialnumber));
        v.push_back(columns[6] + " = " + std::to_string(e.initialserial));

        auto results = db.QueryDatabase(
            fmt::format(
                "UPDATE {} SET {} WHERE {} = {}",
                TableName(),
                Strings::Implode(", ", v),
                PrimaryKey(),
                e.ghost_id
            )
        );

        return (results.Success() ? results.RowsAffected() : 0);
    }

    static HardcoreGhostInventory InsertOne(
        Database& db,
        HardcoreGhostInventory e
    )
    {
        std::vector<std::string> v;

        v.push_back(std::to_string(e.ghost_id));
        v.push_back(std::to_string(e.slotid));
        v.push_back(std::to_string(e.itemid));
        v.push_back(std::to_string(e.charges));
        v.push_back("'" + Strings::Escape(e.custom_data) + "'");
        v.push_back(std::to_string(e.serialnumber));
        v.push_back(std::to_string(e.initialserial));

        auto results = db.QueryDatabase(
            fmt::format(
                "{} VALUES ({})",
                BaseInsert(),
                Strings::Implode(",", v)
            )
        );

        if (results.Success()) {
            e.ghost_id = results.LastInsertedID();
            return e;
        }

        e = NewEntity();

        return e;
    }

    static int InsertMany(
        Database& db,
        const std::vector<HardcoreGhostInventory> &entries
    )
    {
        std::vector<std::string> insert_chunks;

        for (auto &e: entries) {
            std::vector<std::string> v;

            v.push_back(std::to_string(e.ghost_id));
            v.push_back(std::to_string(e.slotid));
            v.push_back(std::to_string(e.itemid));
            v.push_back(std::to_string(e.charges));
            v.push_back("'" + Strings::Escape(e.custom_data) + "'");
            v.push_back(std::to_string(e.serialnumber));
            v.push_back(std::to_string(e.initialserial));

            insert_chunks.push_back("(" + Strings::Implode(",", v) + ")");
        }

        std::vector<std::string> v;

        auto results = db.QueryDatabase(
            fmt::format(
                "{} VALUES {}",
                BaseInsert(),
                Strings::Implode(",", insert_chunks)
            )
        );

        return (results.Success() ? results.RowsAffected() : 0);
    }

    static std::vector<HardcoreGhostInventory> All(Database& db)
    {
        std::vector<HardcoreGhostInventory> all_entries;

        auto results = db.QueryDatabase(
            fmt::format(
                "{}",
                BaseSelect()
            )
        );

        all_entries.reserve(results.RowCount());

        for (auto row = results.begin(); row != results.end(); ++row) {
            HardcoreGhostInventory e{};

            e.ghost_id      = row[0] ? static_cast<int32_t>(atoi(row[0])) : 0;
            e.slotid        = row[1] ? static_cast<uint32_t>(strtoul(row[1], nullptr, 10)) : 0;
            e.itemid        = row[2] ? static_cast<uint32_t>(strtoul(row[2], nullptr, 10)) : 0;
            e.charges       = row[3] ? static_cast<uint16_t>(strtoul(row[3], nullptr, 10)) : 0;
            e.custom_data   = row[4] ? row[4] : "";
            e.serialnumber  = row[5] ? static_cast<int32_t>(atoi(row[5])) : 0;
            e.initialserial = row[6] ? static_cast<int8_t>(atoi(row[6])) : 0;

            all_entries.push_back(e);
        }

        return all_entries;
    }

    static std::vector<HardcoreGhostInventory> GetWhere(Database& db, const std::string &where_filter)
    {
        std::vector<HardcoreGhostInventory> all_entries;

        auto results = db.QueryDatabase(
            fmt::format(
                "{} WHERE {}",
                BaseSelect(),
                where_filter
            )
        );

        all_entries.reserve(results.RowCount());

        for (auto row = results.begin(); row != results.end(); ++row) {
            HardcoreGhostInventory e{};

            e.ghost_id      = row[0] ? static_cast<int32_t>(atoi(row[0])) : 0;
            e.slotid        = row[1] ? static_cast<uint32_t>(strtoul(row[1], nullptr, 10)) : 0;
            e.itemid        = row[2] ? static_cast<uint32_t>(strtoul(row[2], nullptr, 10)) : 0;
            e.charges       = row[3] ? static_cast<uint16_t>(strtoul(row[3], nullptr, 10)) : 0;
            e.custom_data   = row[4] ? row[4] : "";
            e.serialnumber  = row[5] ? static_cast<int32_t>(atoi(row[5])) : 0;
            e.initialserial = row[6] ? static_cast<int8_t>(atoi(row[6])) : 0;

            all_entries.push_back(e);
        }

        return all_entries;
    }

    static int DeleteWhere(Database& db, const std::string &where_filter)
    {
        auto results = db.QueryDatabase(
            fmt::format(
                "DELETE FROM {} WHERE {}",
                TableName(),
                where_filter
            )
        );

        return (results.Success() ? results.RowsAffected() : 0);
    }

    static int Truncate(Database& db)
    {
        auto results = db.QueryDatabase(
            fmt::format(
                "TRUNCATE TABLE {}",
                TableName()
            )
        );

        return (results.Success() ? results.RowsAffected() : 0);
    }

    static int64 GetMaxId(Database& db)
    {
        auto results = db.QueryDatabase(
            fmt::format(
                "SELECT COALESCE(MAX({}), 0) FROM {}",
                PrimaryKey(),
                TableName()
            )
        );

        return (results.Success() && results.begin()[0] ? strtoll(results.begin()[0], nullptr, 10) : 0);
    }

    static int64 Count(Database& db, const std::string &where_filter = "")
    {
        auto results = db.QueryDatabase(
            fmt::format(
                "SELECT COUNT(*) FROM {} {}",
                TableName(),
                (where_filter.empty() ? "" : "WHERE " + where_filter)
            )
        );

        return (results.Success() && results.begin()[0] ? strtoll(results.begin()[0], nullptr, 10) : 0);
    }

};

#endif //EQEMU_BASE_HARDCORE_GHOST_INVENTORY_REPOSITORY_H
