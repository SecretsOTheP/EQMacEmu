#include "../client.h"

#include <cctype>
#include <string>

namespace
{
bool ValidRestoreName(const std::string &name)
{
	if (name.empty() || name.length() > 62) {
		return false;
	}

	for (const auto ch : name) {
		if (!std::isalpha(static_cast<unsigned char>(ch))) {
			return false;
		}
	}

	return true;
}
}

void command_undeletechar(Client *c, const Seperator *sep)
{
	if (!sep->arg[1][0]) {
		c->Message(Chat::White, "Usage: #undeletechar [character] [account] [confirm]");
		return;
	}

	std::string original_name = sep->arg[1];

	if (!ValidRestoreName(original_name)) {
		c->Message(Chat::Red, "Character name must contain letters only.");
		return;
	}

	const auto escaped_name = Strings::Escape(original_name);
	const auto deleted_pattern = "^" + escaped_name + "[0-9]+$";

	// No account supplied: find matching deleted characters and show the GM
	// which account owns each one.
	if (!sep->arg[2][0]) {
		auto results = database.QueryDatabase(StringFormat(
			"SELECT cd.id, a.name, cd.level, cd.name "
			"FROM character_data cd "
			"JOIN account a ON a.id = cd.account_id "
			"WHERE cd.is_deleted = 1 "
			"AND cd.name REGEXP '%s' "
			"ORDER BY cd.id DESC",
			deleted_pattern.c_str()
		));

		if (!results.Success()) {
			c->Message(Chat::Red, "Unable to search deleted characters.");
			return;
		}

		uint32 match_count = 0;
		std::string only_account;

		for (auto row = results.begin(); row != results.end(); ++row) {
			++match_count;
			only_account = row[1] ? row[1] : "";

			c->Message(
				Chat::White,
				"Deleted %s - Account: %s - Level: %s",
				original_name.c_str(),
				row[1] ? row[1] : "Unknown",
				row[2] ? row[2] : "0"
			);
		}

		if (match_count == 0) {
			c->Message(
				Chat::Red,
				"No deleted character named %s was found.",
				original_name.c_str()
			);
			return;
		}

		if (match_count == 1) {
			c->Message(
				Chat::Yellow,
				"Use #undeletechar %s %s to review this restore.",
				original_name.c_str(),
				only_account.c_str()
			);
		}
		else {
			c->Message(
				Chat::Yellow,
				"Multiple deleted characters named %s were found. Use #undeletechar %s [account].",
				original_name.c_str(),
				original_name.c_str()
			);
		}

		return;
	}

	std::string account_name = sep->arg[2];
	const auto escaped_account = Strings::Escape(account_name);

	auto results = database.QueryDatabase(StringFormat(
		"SELECT cd.id, cd.level, cd.name, cd.account_id "
		"FROM character_data cd "
		"JOIN account a ON a.id = cd.account_id "
		"WHERE cd.is_deleted = 1 "
		"AND cd.name REGEXP '%s' "
		"AND a.name = '%s' "
		"ORDER BY cd.id DESC",
		deleted_pattern.c_str(),
		escaped_account.c_str()
	));

	if (!results.Success()) {
		c->Message(Chat::Red, "Unable to search deleted characters.");
		return;
	}

	uint32 match_count = 0;
	uint32 character_id = 0;
	uint32 character_level = 0;
	std::string deleted_name;

	for (auto row = results.begin(); row != results.end(); ++row) {
		++match_count;

		if (match_count == 1) {
			character_id = row[0] ? static_cast<uint32>(atoi(row[0])) : 0;
			character_level = row[1] ? static_cast<uint32>(atoi(row[1])) : 0;
			deleted_name = row[2] ? row[2] : "";
		}
	}

	if (match_count == 0 || character_id == 0) {
		c->Message(
			Chat::Red,
			"No deleted character named %s was found on account %s.",
			original_name.c_str(),
			account_name.c_str()
		);
		return;
	}

	if (match_count > 1) {
		c->Message(
			Chat::Red,
			"More than one deleted %s exists on account %s. Restore refused.",
			original_name.c_str(),
			account_name.c_str()
		);
		return;
	}

	if (character_level < 20) {
		c->Message(
			Chat::Red,
			"%s is level %u. Only deleted characters level 20 or higher are eligible for restoration.",
			original_name.c_str(),
			character_level
		);
		return;
	}

	// Determine which character name can safely be restored.
	std::string restore_name = original_name;

	auto name_check = database.QueryDatabase(StringFormat(
		"SELECT id FROM character_data WHERE name = '%s' LIMIT 1",
		Strings::Escape(restore_name).c_str()
	));

	if (!name_check.Success()) {
		c->Message(Chat::Red, "Unable to check character-name availability.");
		return;
	}

	bool original_taken = false;
	for (auto row = name_check.begin(); row != name_check.end(); ++row) {
		original_taken = true;
		break;
	}

	if (original_taken) {
		restore_name = original_name + "xx";

		auto fallback_check = database.QueryDatabase(StringFormat(
			"SELECT id FROM character_data WHERE name = '%s' LIMIT 1",
			Strings::Escape(restore_name).c_str()
		));

		if (!fallback_check.Success()) {
			c->Message(Chat::Red, "Unable to check fallback character-name availability.");
			return;
		}

		for (auto row = fallback_check.begin(); row != fallback_check.end(); ++row) {
			c->Message(
				Chat::Red,
				"Both %s and %s are already in use. Character was not restored.",
				original_name.c_str(),
				restore_name.c_str()
			);
			return;
		}
	}

	const bool confirmed =
		sep->arg[3][0] &&
		std::string(sep->arg[3]) == "confirm";

	if (!confirmed) {
		c->Message(Chat::White, "Deleted character found:");
		c->Message(Chat::White, "Name: %s", original_name.c_str());
		c->Message(Chat::White, "Account: %s", account_name.c_str());
		c->Message(Chat::White, "Level: %u", character_level);
		c->Message(Chat::White, "Restore name: %s", restore_name.c_str());

		c->Message(
			Chat::Yellow,
			"All existing corpses and everything on those corpses will be permanently deleted."
		);

		if (character_level >= 20) {
			c->Message(
				Chat::Yellow,
				"Level 20+ restore: all carried, cursor, and personal-bank currency will also be permanently removed."
			);

			c->Message(
				Chat::White,
				"Shared bank items and shared platinum are not affected."
			);
		}

		c->Message(
			Chat::Yellow,
			"Use #undeletechar %s %s confirm to restore.",
			original_name.c_str(),
			account_name.c_str()
		);

		return;
	}

	/*
	 * Cleanup happens before the character becomes playable again.
	 *
	 * Corpses are always destroyed on restoration.
	 * Level 20+ characters also lose all character-owned currency.
	 *
	 * account.sharedplat and account_inventory are intentionally untouched.
	 */
	auto transaction_results = database.QueryDatabase("START TRANSACTION");
	if (!transaction_results.Success()) {
		c->Message(Chat::Red, "Unable to start the character restore transaction.");
		return;
	}

	auto rollback_restore = [&](const char *failure) {
		database.QueryDatabase("ROLLBACK");
		c->Message(
			Chat::Red,
			"%s Character remains deleted and no cleanup changes were saved.",
			failure
		);
	};

	// Delete corpse items and live corpse rows inside the transaction. Backup
	// corpse metadata uses MyISAM on this server and is cleaned only after a
	// successful commit because MyISAM changes cannot be rolled back.
	auto corpse_item_results = database.QueryDatabase(StringFormat(
		"DELETE FROM character_corpse_items "
		"WHERE corpse_id IN ("
		"SELECT id FROM character_corpses WHERE charid = %u"
		")",
		character_id
	));

	if (!corpse_item_results.Success()) {
		rollback_restore("Corpse item cleanup failed.");
		return;
	}

	auto corpse_backup_item_results = database.QueryDatabase(StringFormat(
		"DELETE FROM character_corpse_items_backup "
		"WHERE corpse_id IN ("
		"SELECT id FROM character_corpses_backup WHERE charid = %u"
		")",
		character_id
	));

	if (!corpse_backup_item_results.Success()) {
		rollback_restore("Corpse backup item cleanup failed.");
		return;
	}

	auto corpse_results = database.QueryDatabase(StringFormat(
		"DELETE FROM character_corpses WHERE charid = %u",
		character_id
	));

	if (!corpse_results.Success()) {
		rollback_restore("Corpse cleanup failed.");
		return;
	}

	if (character_level >= 20) {
		auto currency_results = database.QueryDatabase(StringFormat(
			"UPDATE character_currency SET "
			"platinum = 0, gold = 0, silver = 0, copper = 0, "
			"platinum_bank = 0, gold_bank = 0, silver_bank = 0, copper_bank = 0, "
			"platinum_cursor = 0, gold_cursor = 0, silver_cursor = 0, copper_cursor = 0 "
			"WHERE id = %u",
			character_id
		));

		if (!currency_results.Success()) {
			rollback_restore("Currency cleanup failed.");
			return;
		}
	}

	auto restore_results = database.QueryDatabase(StringFormat(
		"UPDATE character_data "
		"SET name = '%s', is_deleted = 0 "
		"WHERE id = %u AND is_deleted = 1 AND name = '%s'",
		Strings::Escape(restore_name).c_str(),
		character_id,
		Strings::Escape(deleted_name).c_str()
	));

	if (!restore_results.Success() || restore_results.RowsAffected() != 1) {
		rollback_restore("Character restore failed.");
		return;
	}

	auto commit_results = database.QueryDatabase("COMMIT");
	if (!commit_results.Success()) {
		database.QueryDatabase("ROLLBACK");
		c->Message(Chat::Red, "Character restore transaction could not be committed.");
		return;
	}

	// This table is MyISAM, so remove its now-empty corpse metadata only after
	// the transactional cleanup and character restoration are permanent.
	auto corpse_backup_results = database.QueryDatabase(StringFormat(
		"DELETE FROM character_corpses_backup WHERE charid = %u",
		character_id
	));

	if (!corpse_backup_results.Success()) {
		c->Message(
			Chat::Yellow,
			"%s was restored, but backup corpse metadata cleanup failed for character ID %u.",
			restore_name.c_str(),
			character_id
		);
	}

	// Database cleanup and restoration are now permanent. Remove any copies of
	// this character's corpses that are still loaded in active zones.
	auto corpse_depop_packet = new ServerPacket(
		ServerOP_DepopAllPlayersCorpses,
		sizeof(ServerDepopAllPlayersCorpses_Struct)
	);

	auto corpse_depop = reinterpret_cast<ServerDepopAllPlayersCorpses_Struct *>(
		corpse_depop_packet->pBuffer
	);

	corpse_depop->CharacterID = character_id;
	corpse_depop->ZoneID = zone->GetZoneID();
	corpse_depop->GuildID = zone->GetGuildID();

	worldserver.SendPacket(corpse_depop_packet);
	safe_delete(corpse_depop_packet);

	// The world packet intentionally skips the originating zone, so remove
	// this character's corpses from the current zone directly as well.
	entity_list.RemoveAllCorpsesByCharID(character_id);

	LogInfo(
		"GM [{}] restored deleted character [{}] (ID [{}], account [{}]) as [{}]",
		c->GetName(),
		original_name,
		character_id,
		account_name,
		restore_name
	);

	c->Message(
		Chat::Green,
		"%s successfully restored on account %s as %s.",
		original_name.c_str(),
		account_name.c_str(),
		restore_name.c_str()
	);
}
