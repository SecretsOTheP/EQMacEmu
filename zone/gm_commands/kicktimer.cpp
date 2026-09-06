#include "../client.h"

void command_kicktimer(Client *c, const Seperator *sep)
{
        if (!c) {
                return;
        }

        if (sep->argnum >= 1 && !strcasecmp(sep->arg[1], "list")) {
            auto results = database.QueryDatabase(
                    "SELECT short_name, afk_kick_timer "
                    "FROM zone "
                    "WHERE afk_kick_timer > 0 "
                    "ORDER BY short_name"
            );

            if (!results.Success()) {
                    c->Message(
                            Chat::Red,
                            "Database error while listing zone kick timers."
                    );
                    return;
            }

            if (results.RowCount() == 0) {
                    c->Message(
                            Chat::White,
                            "No zone kick timers are currently enabled."
                    );
                    return;
            }

            c->Message(
                    Chat::White,
                    "Zone kick timers currently enabled:"
            );

            for (auto row : results) {
                    uint32 seconds = Strings::ToUnsignedInt(row[1]);

                    c->Message(
                            Chat::White,
                            fmt::format(
                                    "{} - {}",
                                    row[0],
                                    Strings::SecondsToTime(seconds)
                            ).c_str()
                    );
            }

            return;
    }

    if (sep->argnum < 2 || !sep->IsNumber(2)) {
                c->Message(
                        Chat::White,
                        "Usage: #kicktimer [zone_short_name] [minutes]"
                );
                c->Message(
                        Chat::White,
                        "Use 0 minutes to disable the zone kick timer."
                );
                return;
        }

        std::string zone_short_name = Strings::ToLower(sep->arg[1]);
        uint64 minutes = strtoull(sep->arg[2], nullptr, 10);

        constexpr uint64 maximum_minutes = 60;

        if (minutes > maximum_minutes) {
                c->Message(
                        Chat::Red,
                        fmt::format(
                                "Minutes must be between 0 and {}.",
                                maximum_minutes
                        ).c_str()
                );
                return;
        }

        uint32 kick_timer_seconds =
                static_cast<uint32>(minutes * 60);

        auto zone_check = database.QueryDatabase(
                fmt::format(
                        "SELECT short_name FROM zone "
                        "WHERE short_name = '{}' LIMIT 1",
                        Strings::Escape(zone_short_name)
                )
        );

        if (!zone_check.Success()) {
                c->Message(
                        Chat::Red,
                        "Database error while checking the zone."
                );
                return;
        }

        if (zone_check.RowCount() != 1) {
                c->Message(
                        Chat::Red,
                        fmt::format(
                                "Zone '{}' was not found. Use the zone short name.",
                                zone_short_name
                        ).c_str()
                );
                return;
        }

        auto update_results = database.QueryDatabase(
                fmt::format(
                        "UPDATE zone SET afk_kick_timer = {} "
                        "WHERE short_name = '{}'",
                        kick_timer_seconds,
                        Strings::Escape(zone_short_name)
                )
        );

        if (!update_results.Success()) {
                c->Message(
                        Chat::Red,
                        fmt::format(
                                "Failed to update the kick timer for '{}'.",
                                zone_short_name
                        ).c_str()
                );
                return;
        }

        auto pack = new ServerPacket(
                ServerOP_ReloadZoneKickTimer,
                sizeof(ReloadZoneKickTimer_Struct)
        );

        auto* reload = (ReloadZoneKickTimer_Struct*)pack->pBuffer;

        strn0cpy(
                reload->zone_short_name,
                zone_short_name.c_str(),
                sizeof(reload->zone_short_name)
        );

        worldserver.SendPacket(pack);
        safe_delete(pack);

        if (minutes == 0) {
                c->Message(
                        Chat::Lime,
                        fmt::format(
                                "Kick timer for '{}' has been disabled. "
                                "If the zone is currently running, the change will apply immediately; "
                                "otherwise it will take effect the next time the zone starts.",
                                zone_short_name
                        ).c_str()
                );
                return;
        }

        c->Message(
                Chat::Lime,
                fmt::format(
                        "Kick timer for '{}' changed to {} minute{}. "
                        "If the zone is currently running, the change will apply immediately; "
                        "otherwise it will take effect the next time the zone starts.",
                        zone_short_name,
                        minutes,
                        minutes == 1 ? "" : "s"
                ).c_str()
        );

}
