/* Fix respawn timers for Epic mobs in Plane of Fear */
update npc_types set instance_spawn_timer_override = 3600000 where name = 'Wraith_of_a_Shissir';
update npc_types set instance_spawn_timer_override = 3600000 where name = 'a_broken_golem';