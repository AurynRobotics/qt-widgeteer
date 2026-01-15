add_executable(widgeteer_sample
  main.cpp
)

target_link_libraries(widgeteer_sample PRIVATE
  widgeteer
  Qt6::Core
  Qt6::Widgets
)
