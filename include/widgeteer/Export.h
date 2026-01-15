#pragma once

#include <QtCore/qglobal.h>

#if defined(WIDGETEER_SHARED_LIBRARY)
#if defined(WIDGETEER_BUILDING_LIBRARY)
#define WIDGETEER_EXPORT Q_DECL_EXPORT
#else
#define WIDGETEER_EXPORT Q_DECL_IMPORT
#endif
#else
#define WIDGETEER_EXPORT
#endif
