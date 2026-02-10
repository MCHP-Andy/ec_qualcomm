
target_sources(app PRIVATE btn.c)

target_sources_ifdef(CONFIG_PM app PRIVATE power.c)
