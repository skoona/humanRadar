set(SOURCES main.c rgb_panel.c radar_panel.c)
set(COMPONENT_USED spiffs esp_timer esp_psram) 
idf_component_register(
    SRCS ${SOURCES}
    REQUIRES wifi_network
	PRIV_REQUIRES ${COMPONENT_USED}
    INCLUDE_DIRS "include")
