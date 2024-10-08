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

#ifndef EQEMU_BASE_NPC_TYPES_METADATA_REPOSITORY_H
#define EQEMU_BASE_NPC_TYPES_METADATA_REPOSITORY_H

#include "../../database.h"
#include "../../strings.h"
#include <ctime>

class BaseNpcTypesMetadataRepository {
public:
	struct NpcTypesMetadata {
		int32_t npc_types_id;
		int8_t  isPKMob;
		int8_t  isNamedMob;
		int8_t  isRaidTarget;
		int8_t  isCreatedMob;
		int8_t  isCustomFeatureNPC;
	};

	static std::string PrimaryKey()
	{
		return std::string("npc_types_id");
	}

	static std::vector<std::string> Columns()
	{
		return {
			"npc_types_id",
			"isPKMob",
			"isNamedMob",
			"isRaidTarget",
			"isCreatedMob",
			"isCustomFeatureNPC",
		};
	}

	static std::vector<std::string> SelectColumns()
	{
		return {
			"npc_types_id",
			"isPKMob",
			"isNamedMob",
			"isRaidTarget",
			"isCreatedMob",
			"isCustomFeatureNPC",
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
		return std::string("npc_types_metadata");
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

	static NpcTypesMetadata NewEntity()
	{
		NpcTypesMetadata e{};

		e.npc_types_id       = 0;
		e.isPKMob            = 0;
		e.isNamedMob         = 0;
		e.isRaidTarget       = 0;
		e.isCreatedMob       = 0;
		e.isCustomFeatureNPC = 0;

		return e;
	}

	static NpcTypesMetadata GetNpcTypesMetadata(
		const std::vector<NpcTypesMetadata> &npc_types_metadatas,
		int npc_types_metadata_id
	)
	{
		for (auto &npc_types_metadata : npc_types_metadatas) {
			if (npc_types_metadata.npc_types_id == npc_types_metadata_id) {
				return npc_types_metadata;
			}
		}

		return NewEntity();
	}

	static NpcTypesMetadata FindOne(
		Database& db,
		int npc_types_metadata_id
	)
	{
		auto results = db.QueryDatabase(
			fmt::format(
				"{} WHERE {} = {} LIMIT 1",
				BaseSelect(),
				PrimaryKey(),
				npc_types_metadata_id
			)
		);

		auto row = results.begin();
		if (results.RowCount() == 1) {
			NpcTypesMetadata e{};

			e.npc_types_id       = row[0] ? static_cast<int32_t>(atoi(row[0])) : 0;
			e.isPKMob            = row[1] ? static_cast<int8_t>(atoi(row[1])) : 0;
			e.isNamedMob         = row[2] ? static_cast<int8_t>(atoi(row[2])) : 0;
			e.isRaidTarget       = row[3] ? static_cast<int8_t>(atoi(row[3])) : 0;
			e.isCreatedMob       = row[4] ? static_cast<int8_t>(atoi(row[4])) : 0;
			e.isCustomFeatureNPC = row[5] ? static_cast<int8_t>(atoi(row[5])) : 0;

			return e;
		}

		return NewEntity();
	}

	static int DeleteOne(
		Database& db,
		int npc_types_metadata_id
	)
	{
		auto results = db.QueryDatabase(
			fmt::format(
				"DELETE FROM {} WHERE {} = {}",
				TableName(),
				PrimaryKey(),
				npc_types_metadata_id
			)
		);

		return (results.Success() ? results.RowsAffected() : 0);
	}

	static int UpdateOne(
		Database& db,
		const NpcTypesMetadata &e
	)
	{
		std::vector<std::string> v;

		auto columns = Columns();

		v.push_back(columns[0] + " = " + std::to_string(e.npc_types_id));
		v.push_back(columns[1] + " = " + std::to_string(e.isPKMob));
		v.push_back(columns[2] + " = " + std::to_string(e.isNamedMob));
		v.push_back(columns[3] + " = " + std::to_string(e.isRaidTarget));
		v.push_back(columns[4] + " = " + std::to_string(e.isCreatedMob));
		v.push_back(columns[5] + " = " + std::to_string(e.isCustomFeatureNPC));

		auto results = db.QueryDatabase(
			fmt::format(
				"UPDATE {} SET {} WHERE {} = {}",
				TableName(),
				Strings::Implode(", ", v),
				PrimaryKey(),
				e.npc_types_id
			)
		);

		return (results.Success() ? results.RowsAffected() : 0);
	}

	static NpcTypesMetadata InsertOne(
		Database& db,
		NpcTypesMetadata e
	)
	{
		std::vector<std::string> v;

		v.push_back(std::to_string(e.npc_types_id));
		v.push_back(std::to_string(e.isPKMob));
		v.push_back(std::to_string(e.isNamedMob));
		v.push_back(std::to_string(e.isRaidTarget));
		v.push_back(std::to_string(e.isCreatedMob));
		v.push_back(std::to_string(e.isCustomFeatureNPC));

		auto results = db.QueryDatabase(
			fmt::format(
				"{} VALUES ({})",
				BaseInsert(),
				Strings::Implode(",", v)
			)
		);

		if (results.Success()) {
			e.npc_types_id = results.LastInsertedID();
			return e;
		}

		e = NewEntity();

		return e;
	}

	static int InsertMany(
		Database& db,
		const std::vector<NpcTypesMetadata> &entries
	)
	{
		std::vector<std::string> insert_chunks;

		for (auto &e: entries) {
			std::vector<std::string> v;

			v.push_back(std::to_string(e.npc_types_id));
			v.push_back(std::to_string(e.isPKMob));
			v.push_back(std::to_string(e.isNamedMob));
			v.push_back(std::to_string(e.isRaidTarget));
			v.push_back(std::to_string(e.isCreatedMob));
			v.push_back(std::to_string(e.isCustomFeatureNPC));

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

	static std::vector<NpcTypesMetadata> All(Database& db)
	{
		std::vector<NpcTypesMetadata> all_entries;

		auto results = db.QueryDatabase(
			fmt::format(
				"{}",
				BaseSelect()
			)
		);

		all_entries.reserve(results.RowCount());

		for (auto row = results.begin(); row != results.end(); ++row) {
			NpcTypesMetadata e{};

			e.npc_types_id       = row[0] ? static_cast<int32_t>(atoi(row[0])) : 0;
			e.isPKMob            = row[1] ? static_cast<int8_t>(atoi(row[1])) : 0;
			e.isNamedMob         = row[2] ? static_cast<int8_t>(atoi(row[2])) : 0;
			e.isRaidTarget       = row[3] ? static_cast<int8_t>(atoi(row[3])) : 0;
			e.isCreatedMob       = row[4] ? static_cast<int8_t>(atoi(row[4])) : 0;
			e.isCustomFeatureNPC = row[5] ? static_cast<int8_t>(atoi(row[5])) : 0;

			all_entries.push_back(e);
		}

		return all_entries;
	}

	static std::vector<NpcTypesMetadata> GetWhere(Database& db, const std::string &where_filter)
	{
		std::vector<NpcTypesMetadata> all_entries;

		auto results = db.QueryDatabase(
			fmt::format(
				"{} WHERE {}",
				BaseSelect(),
				where_filter
			)
		);

		all_entries.reserve(results.RowCount());

		for (auto row = results.begin(); row != results.end(); ++row) {
			NpcTypesMetadata e{};

			e.npc_types_id       = row[0] ? static_cast<int32_t>(atoi(row[0])) : 0;
			e.isPKMob            = row[1] ? static_cast<int8_t>(atoi(row[1])) : 0;
			e.isNamedMob         = row[2] ? static_cast<int8_t>(atoi(row[2])) : 0;
			e.isRaidTarget       = row[3] ? static_cast<int8_t>(atoi(row[3])) : 0;
			e.isCreatedMob       = row[4] ? static_cast<int8_t>(atoi(row[4])) : 0;
			e.isCustomFeatureNPC = row[5] ? static_cast<int8_t>(atoi(row[5])) : 0;

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

#endif //EQEMU_BASE_NPC_TYPES_METADATA_REPOSITORY_H
