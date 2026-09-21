idf_component_register(
    SRCS "main.c"
         "liuyao_app.c"
         "liuyao_engine.c"
         "liuyao_calendar.c"
         "liuyao_data.c"
         "liuyao_reading.c"
         "liuyao_theme.c"
         "liuyao_pages_flow.c"
         "liuyao_pages_cast.c"
         "liuyao_pages_result.c"
         "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/liuyao_font_16.c"
         "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/liuyao_font_24.c"
         "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/liuyao_font_48.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_hw_support esp_timer nvs_flash
)
