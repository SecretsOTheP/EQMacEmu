/**
 * DO NOT MODIFY THIS FILE
 *
 * This repository was automatically generated and is NOT to be modified directly.
 * Any repository modifications are meant to be made to the repository extending the base.
 * Any modifications to base repositories are to be made by the generator only
 *
 * @generator ./utils/scripts/generators/repository-generator.pl
 * @docs https://docs.eqemu.io/developer/repositories
 */

#ifndef EQEMU_BASE_HARDCOREGHOST_DATA_REPOSITORY_H
#define EQEMU_BASE_HARDCOREGHOST_DATA_REPOSITORY_H

#include "../../database.h"
#include "../../strings.h"
#include <ctime>

class BaseHardcoreGhostDataRepository {
public:
    struct HardcoreGhostData {
        uint32_t    ghost_id;
        std::string name;
        std::string last_name;
        std::string title;
        std::string suffix;
        int8_t      gender;
        int16_t     race;
        int8_t      class_;
        uint8_t     level;
        uint32_t    deity;
        float       size;
        int32_t     face;
        int32_t     hair_color;
        int32_t     hair_style;
        int32_t     beard;
        int32_t     beard_color;
        int32_t     eye_color_1;
        int32_t     eye_color_2;
    };

    static std::string PrimaryKey()
    {
        return std::string("ghost_id");
    }

    static std::vector<std::string> Columns()
    {
        return {
            "ghost_id",
            "name",
            "last_name",
            "title",
            "suffix",
            "gender",
            "race",
            "`class`",
            "level",
            "deity",
            "size",
            "face",
            "hair_color",
            "hair_style",
            "beard",
            "beard_color",
            "eye_color_1",
            "eye_color_2"
        };
    }

    static std::vector<std::string> SelectColumns()
    {
        return {
            "ghost_id",
            "name",
            "last_name",
            "title",
            "suffix",
            "gender",
            "race",
            "`class`",
            "level",
            "deity",
            "size",
            "face",
            "hair_color",
            "hair_style",
            "beard",
            "beard_color",
            "eye_color_1",
            "eye_color_2"
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
        return std::string("hardcore_ghosts");
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

    static HardcoreGhostData NewEntity()
    {
        HardcoreGhostData e{};

        e.ghost_id                 = 0;
        e.name                   = "";
        e.last_name              = "";
        e.title                  = "";
        e.suffix                 = "";
        e.gender                 = 0;
        e.race                   = 0;
        e.class_                 = 0;
        e.level                  = 0;
        e.deity                  = 0;
        e.size                   = 0;
        e.face                   = 1;
        e.hair_color             = 1;
        e.hair_style             = 1;
        e.beard                  = 0;
        e.beard_color            = 1;
        e.eye_color_1            = 1;
        e.eye_color_2            = 1;

        return e;
    }

    static HardcoreGhostData GetGhostData(
        const std::vector<HardcoreGhostData> &ghost_datas,
        int ghost_data_id
    )
    {
        for (auto &ghost_data : ghost_datas) {
            if (ghost_data.ghost_id == ghost_data_id) {
                return ghost_data;
            }
        }

        return NewEntity();
    }

    static HardcoreGhostData FindOne(
        Database& db,
        int ghost_data_id
    )
    {
        auto results = db.QueryDatabase(
            fmt::format(
                "{} WHERE {} = {} LIMIT 1",
                BaseSelect(),
                PrimaryKey(),
                ghost_data_id
            )
        );

        auto row = results.begin();
        if (results.RowCount() == 1) {
            HardcoreGhostData e{};

            e.ghost_id               = row[0] ? static_cast<uint32_t>(strtoul(row[0], nullptr, 10)) : 0;
            e.name                   = row[1] ? row[1] : "";
            e.last_name              = row[2] ? row[2] : "";
            e.title                  = row[3] ? row[3] : "";
            e.suffix                 = row[4] ? row[4] : "";
            e.gender                 = row[5] ? static_cast<int8_t>(atoi(row[5])) : 0;
            e.race                   = row[6] ? static_cast<int16_t>(atoi(row[6])) : 0;
            e.class_                 = row[7] ? static_cast<int8_t>(atoi(row[7])) : 0;
            e.level                  = row[8] ? static_cast<uint8_t>(strtoul(row[8], nullptr, 10)) : 0;
            e.deity                  = row[9] ? static_cast<uint32_t>(strtoul(row[9], nullptr, 10)) : 0;
            e.size                   = row[10] ? strtof(row[10], nullptr) : 0;
            e.face                   = row[11] ? static_cast<int32_t>(atoi(row[11])) : 1;
            e.hair_color             = row[12] ? static_cast<int32_t>(atoi(row[12])) : 1;
            e.hair_style             = row[13] ? static_cast<int32_t>(atoi(row[13])) : 1;
            e.beard                  = row[14] ? static_cast<int32_t>(atoi(row[14])) : 0;
            e.beard_color            = row[15] ? static_cast<int32_t>(atoi(row[15])) : 1;
            e.eye_color_1            = row[16] ? static_cast<int32_t>(atoi(row[16])) : 1;
            e.eye_color_2            = row[17] ? static_cast<int32_t>(atoi(row[17])) : 1;
            return e;
        }

        return NewEntity();
    }

    static int DeleteOne(
        Database& db,
        int ghost_data_id
    )
    {
        auto results = db.QueryDatabase(
            fmt::format(
                "DELETE FROM {} WHERE {} = {}",
                TableName(),
                PrimaryKey(),
                ghost_data_id
            )
        );

        return (results.Success() ? results.RowsAffected() : 0);
    }

    static int UpdateOne(
        Database& db,
        const HardcoreGhostData &e
    )
    {
        std::vector<std::string> v;

        auto columns = Columns();

        v.push_back(columns[0] + " = '" + Strings::Escape(e.name) + "'");
        v.push_back(columns[1] + " = '" + Strings::Escape(e.last_name) + "'");
        v.push_back(columns[2] + " = '" + Strings::Escape(e.title) + "'");
        v.push_back(columns[3] + " = '" + Strings::Escape(e.suffix) + "'");
        v.push_back(columns[4] + " = " + std::to_string(e.gender));
        v.push_back(columns[5] + " = " + std::to_string(e.race));
        v.push_back(columns[6] + " = " + std::to_string(e.class_));
        v.push_back(columns[7] + " = " + std::to_string(e.level));
        v.push_back(columns[8] + " = " + std::to_string(e.deity));
        v.push_back(columns[9] + " = " + std::to_string(e.size));
        v.push_back(columns[10] + " = " + std::to_string(e.face));
        v.push_back(columns[11] + " = " + std::to_string(e.hair_color));
        v.push_back(columns[12] + " = " + std::to_string(e.hair_style));
        v.push_back(columns[13] + " = " + std::to_string(e.beard));
        v.push_back(columns[14] + " = " + std::to_string(e.beard_color));
        v.push_back(columns[15] + " = " + std::to_string(e.eye_color_1));
        v.push_back(columns[16] + " = " + std::to_string(e.eye_color_2));


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

    static HardcoreGhostData InsertOne(
        Database& db,
        HardcoreGhostData e
    )
    {
        std::vector<std::string> v;

        v.push_back(std::to_string(e.ghost_id));
        v.push_back("'" + Strings::Escape(e.name) + "'");
        v.push_back("'" + Strings::Escape(e.last_name) + "'");
        v.push_back("'" + Strings::Escape(e.title) + "'");
        v.push_back("'" + Strings::Escape(e.suffix) + "'");
        v.push_back(std::to_string(e.gender));
        v.push_back(std::to_string(e.race));
        v.push_back(std::to_string(e.class_));
        v.push_back(std::to_string(e.level));
        v.push_back(std::to_string(e.deity));
        v.push_back(std::to_string(e.size));
        v.push_back(std::to_string(e.face));
        v.push_back(std::to_string(e.hair_color));
        v.push_back(std::to_string(e.hair_style));
        v.push_back(std::to_string(e.beard));
        v.push_back(std::to_string(e.beard_color));
        v.push_back(std::to_string(e.eye_color_1));
        v.push_back(std::to_string(e.eye_color_2));

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
        const std::vector<HardcoreGhostData> &entries
    )
    {
        std::vector<std::string> insert_chunks;

        for (auto &e: entries) {
            std::vector<std::string> v;

            v.push_back(std::to_string(e.ghost_id));
            v.push_back("'" + Strings::Escape(e.name) + "'");
            v.push_back("'" + Strings::Escape(e.last_name) + "'");
            v.push_back("'" + Strings::Escape(e.title) + "'");
            v.push_back("'" + Strings::Escape(e.suffix) + "'");
            v.push_back(std::to_string(e.gender));
            v.push_back(std::to_string(e.race));
            v.push_back(std::to_string(e.class_));
            v.push_back(std::to_string(e.level));
            v.push_back(std::to_string(e.deity));
            v.push_back(std::to_string(e.size));
            v.push_back(std::to_string(e.face));
            v.push_back(std::to_string(e.hair_color));
            v.push_back(std::to_string(e.hair_style));
            v.push_back(std::to_string(e.beard));
            v.push_back(std::to_string(e.beard_color));
            v.push_back(std::to_string(e.eye_color_1));
            v.push_back(std::to_string(e.eye_color_2));

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

    static std::vector<HardcoreGhostData> All(Database& db)
    {
        std::vector<HardcoreGhostData> all_entries;

        auto results = db.QueryDatabase(
            fmt::format(
                "{}",
                BaseSelect()
            )
        );

        all_entries.reserve(results.RowCount());

        for (auto row = results.begin(); row != results.end(); ++row) {
            HardcoreGhostData e{};

            e.ghost_id               = row[0] ? static_cast<uint32_t>(strtoul(row[0], nullptr, 10)) : 0;
            e.name                   = row[1] ? row[1] : "";
            e.last_name              = row[2] ? row[2] : "";
            e.title                  = row[3] ? row[3] : "";
            e.suffix                 = row[4] ? row[4] : "";
            e.gender                 = row[5] ? static_cast<int8_t>(atoi(row[5])) : 0;
            e.race                   = row[6] ? static_cast<int16_t>(atoi(row[6])) : 0;
            e.class_                 = row[7] ? static_cast<int8_t>(atoi(row[7])) : 0;
            e.level                  = row[8] ? static_cast<uint8_t>(strtoul(row[8], nullptr, 10)) : 0;
            e.deity                  = row[9] ? static_cast<uint32_t>(strtoul(row[9], nullptr, 10)) : 0;
            e.size                   = row[10] ? strtof(row[10], nullptr) : 0;
            e.face                   = row[11] ? static_cast<int32_t>(atoi(row[11])) : 1;
            e.hair_color             = row[12] ? static_cast<int32_t>(atoi(row[12])) : 1;
            e.hair_style             = row[13] ? static_cast<int32_t>(atoi(row[13])) : 1;
            e.beard                  = row[14] ? static_cast<int32_t>(atoi(row[14])) : 0;
            e.beard_color            = row[15] ? static_cast<int32_t>(atoi(row[15])) : 1;
            e.eye_color_1            = row[16] ? static_cast<int32_t>(atoi(row[16])) : 1;
            e.eye_color_2            = row[17] ? static_cast<int32_t>(atoi(row[17])) : 1;

            all_entries.push_back(e);
        }

        return all_entries;
    }

    static std::vector<HardcoreGhostData> GetWhere(Database& db, const std::string &where_filter)
    {
        std::vector<HardcoreGhostData> all_entries;

        auto results = db.QueryDatabase(
            fmt::format(
                "{} WHERE {}",
                BaseSelect(),
                where_filter
            )
        );

        all_entries.reserve(results.RowCount());

        for (auto row = results.begin(); row != results.end(); ++row) {
            HardcoreGhostData e{};

            e.ghost_id               = row[0] ? static_cast<uint32_t>(strtoul(row[0], nullptr, 10)) : 0;
            e.name                   = row[1] ? row[1] : "";
            e.last_name              = row[2] ? row[2] : "";
            e.title                  = row[3] ? row[3] : "";
            e.suffix                 = row[4] ? row[4] : "";
            e.gender                 = row[5] ? static_cast<int8_t>(atoi(row[5])) : 0;
            e.race                   = row[6] ? static_cast<int16_t>(atoi(row[6])) : 0;
            e.class_                 = row[7] ? static_cast<int8_t>(atoi(row[7])) : 0;
            e.level                  = row[8] ? static_cast<uint8_t>(strtoul(row[8], nullptr, 10)) : 0;
            e.deity                  = row[9] ? static_cast<uint32_t>(strtoul(row[9], nullptr, 10)) : 0;
            e.size                   = row[10] ? strtof(row[10], nullptr) : 0;
            e.face                   = row[11] ? static_cast<int32_t>(atoi(row[11])) : 1;
            e.hair_color             = row[12] ? static_cast<int32_t>(atoi(row[12])) : 1;
            e.hair_style             = row[13] ? static_cast<int32_t>(atoi(row[13])) : 1;
            e.beard                  = row[14] ? static_cast<int32_t>(atoi(row[14])) : 0;
            e.beard_color            = row[15] ? static_cast<int32_t>(atoi(row[15])) : 1;
            e.eye_color_1            = row[16] ? static_cast<int32_t>(atoi(row[16])) : 1;
            e.eye_color_2            = row[17] ? static_cast<int32_t>(atoi(row[17])) : 1;

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

    static int64_t GetMaxId(Database& db)
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

    static int64_t Count(Database& db, const std::string &where_filter = "")
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

    static std::string BaseReplace()
    {
        return fmt::format(
            "REPLACE INTO {} ({}) ",
            TableName(),
            ColumnsRaw()
        );
    }

    static int ReplaceOne(
        Database& db,
        const HardcoreGhostData &e
    )
    {
        std::vector<std::string> v;

        v.push_back(std::to_string(e.ghost_id));
        v.push_back("'" + Strings::Escape(e.name) + "'");
        v.push_back("'" + Strings::Escape(e.last_name) + "'");
        v.push_back("'" + Strings::Escape(e.title) + "'");
        v.push_back("'" + Strings::Escape(e.suffix) + "'");
        v.push_back(std::to_string(e.gender));
        v.push_back(std::to_string(e.race));
        v.push_back(std::to_string(e.class_));
        v.push_back(std::to_string(e.level));
        v.push_back(std::to_string(e.deity));
        v.push_back(std::to_string(e.size));
        v.push_back(std::to_string(e.face));
        v.push_back(std::to_string(e.hair_color));
        v.push_back(std::to_string(e.hair_style));
        v.push_back(std::to_string(e.beard));
        v.push_back(std::to_string(e.beard_color));
        v.push_back(std::to_string(e.eye_color_1));
        v.push_back(std::to_string(e.eye_color_2));

        auto results = db.QueryDatabase(
            fmt::format(
                "{} VALUES ({})",
                BaseReplace(),
                Strings::Implode(",", v)
            )
        );

        return (results.Success() ? results.RowsAffected() : 0);
    }

    static int ReplaceMany(
        Database& db,
        const std::vector<HardcoreGhostData> &entries
    )
    {
        std::vector<std::string> insert_chunks;

        for (auto &e: entries) {
            std::vector<std::string> v;

            v.push_back(std::to_string(e.ghost_id));
            v.push_back("'" + Strings::Escape(e.name) + "'");
            v.push_back("'" + Strings::Escape(e.last_name) + "'");
            v.push_back("'" + Strings::Escape(e.title) + "'");
            v.push_back("'" + Strings::Escape(e.suffix) + "'");
            v.push_back(std::to_string(e.gender));
            v.push_back(std::to_string(e.race));
            v.push_back(std::to_string(e.class_));
            v.push_back(std::to_string(e.level));
            v.push_back(std::to_string(e.deity));
            v.push_back(std::to_string(e.size));
            v.push_back(std::to_string(e.face));
            v.push_back(std::to_string(e.hair_color));
            v.push_back(std::to_string(e.hair_style));
            v.push_back(std::to_string(e.beard));
            v.push_back(std::to_string(e.beard_color));
            v.push_back(std::to_string(e.eye_color_1));
            v.push_back(std::to_string(e.eye_color_2));

            insert_chunks.push_back("(" + Strings::Implode(",", v) + ")");
        }

        std::vector<std::string> v;

        auto results = db.QueryDatabase(
            fmt::format(
                "{} VALUES {}",
                BaseReplace(),
                Strings::Implode(",", insert_chunks)
            )
        );

        return (results.Success() ? results.RowsAffected() : 0);
    }
};

#endif //EQEMU_BASE_HARDCOREGHOST_DATA_REPOSITORY_H
