// scenes/protopirate_scene_encode.c
#include "../protopirate_app_i.h"
#include "../helpers/protopirate_storage.h"
#include <notification/notification_messages.h>

#define TAG "ProtoPirateEncode"

typedef enum {
    SubmenuIndexEncodeLoadFile,
    SubmenuIndexEncodeManualEntry,
    SubmenuIndexEncodeConfig,
    SubmenuIndexEncodeBack,
} SubmenuIndexEncode;

static void protopirate_scene_encode_submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    ProtoPirateApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void protopirate_scene_encode_on_enter(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = context;

    submenu_add_item(
        app->submenu,
        "Load Capture File",
        SubmenuIndexEncodeLoadFile,
        protopirate_scene_encode_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Manual Entry",
        SubmenuIndexEncodeManualEntry,
        protopirate_scene_encode_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Configuration",
        SubmenuIndexEncodeConfig,
        protopirate_scene_encode_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Back",
        SubmenuIndexEncodeBack,
        protopirate_scene_encode_submenu_callback,
        app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, ProtoPirateSceneEncode));

    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewSubmenu);
}

bool protopirate_scene_encode_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    ProtoPirateApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SubmenuIndexEncodeLoadFile:
            // Navigate to saved files to load for encoding
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneSaved);
            consumed = true;
            break;

        case SubmenuIndexEncodeManualEntry:
            // Navigate to encode config for manual entry
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneEncodeConfig);
            consumed = true;
            break;

        case SubmenuIndexEncodeConfig:
            // Navigate to encode configuration
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneEncodeConfig);
            consumed = true;
            break;

        case SubmenuIndexEncodeBack:
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;
        }
        scene_manager_set_scene_state(app->scene_manager, ProtoPirateSceneEncode, event.event);
    }

    return consumed;
}

void protopirate_scene_encode_on_exit(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = context;
    submenu_reset(app->submenu);
}
