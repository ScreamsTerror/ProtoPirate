// scenes/protopirate_scene_encode_config.c
#include "../protopirate_app_i.h"
#include "../helpers/protopirate_storage.h"
#include <notification/notification_messages.h>

#define TAG "ProtoPirateEncodeConfig"

typedef enum {
    EncodeConfigItemProtocol,
    EncodeConfigItemSerial,
    EncodeConfigItemButton,
    EncodeConfigItemCounter,
    EncodeConfigItemFrequency,
    EncodeConfigItemTransmit,
    EncodeConfigItemCount,
} EncodeConfigItem;

typedef struct {
    VariableItem* variable_item;
    uint8_t value_index;
    const char** options;
    uint8_t option_count;
} EncodeConfigContext;

static EncodeConfigContext config_context[EncodeConfigItemCount];

// Protocol options
static const char* protocol_options[] = {
    "Kia V0",
    "Ford V0", 
    "Subaru",
    "Suzuki",
    "VW",
    "Toyota V0",
    "Honda V0",
    "Nissan V0",
    "BMW V0",
    "Mercedes V0",
    "Hyundai V0",
};

static const char* button_options[] = {
    "Unlock (0x1)",
    "Lock (0x2)", 
    "Trunk (0x4)",
    "Panic (0x8)",
};

static const char* frequency_options[] = {
    "433.92 MHz",
    "315.00 MHz",
    "868.35 MHz",
};

static void protopirate_scene_encode_config_protocol_callback(VariableItem* item) {
    EncodeConfigContext* context = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    context->value_index = index;
    variable_item_set_current_value_text(item, protocol_options[index]);
}

static void protopirate_scene_encode_config_button_callback(VariableItem* item) {
    EncodeConfigContext* context = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    context->value_index = index;
    variable_item_set_current_value_text(item, button_options[index]);
}

static void protopirate_scene_encode_config_frequency_callback(VariableItem* item) {
    EncodeConfigContext* context = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    context->value_index = index;
    variable_item_set_current_value_text(item, frequency_options[index]);
}

static void protopirate_scene_encode_config_serial_callback(VariableItem* item) {
    // This would open a text input dialog in a real implementation
    variable_item_set_current_value_text(item, "1234567");
}

static void protopirate_scene_encode_config_counter_callback(VariableItem* item) {
    // This would open a number input dialog in a real implementation
    variable_item_set_current_value_text(item, "1000");
}

static void protopirate_scene_encode_config_transmit_callback(VariableItem* item) {
    EncodeConfigContext* context = variable_item_get_context(item);
    ProtoPirateApp* app = context->variable_item ? 
        (ProtoPirateApp*)variable_item_get_context(context->variable_item) : NULL;
    
    if(app) {
        // Trigger transmission
        view_dispatcher_send_custom_event(app->view_dispatcher, EncodeConfigItemTransmit);
    }
}

void protopirate_scene_encode_config_on_enter(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = context;
    VariableItemList* variable_item_list = app->variable_item_list;

    // Clear the list
    variable_item_list_reset(variable_item_list);

    // Protocol selection
    VariableItem* protocol_item = variable_item_list_add(
        variable_item_list, "Protocol:", COUNT_OF(protocol_options), 
        protopirate_scene_encode_config_protocol_callback, &config_context[EncodeConfigItemProtocol]);
    config_context[EncodeConfigItemProtocol].variable_item = protocol_item;
    config_context[EncodeConfigItemProtocol].options = protocol_options;
    config_context[EncodeConfigItemProtocol].option_count = COUNT_OF(protocol_options);
    config_context[EncodeConfigItemProtocol].value_index = 0;
    variable_item_set_current_value_index(protocol_item, 0);
    variable_item_set_current_value_text(protocol_item, protocol_options[0]);

    // Serial number
    VariableItem* serial_item = variable_item_list_add(
        variable_item_list, "Serial:", 1, 
        protopirate_scene_encode_config_serial_callback, &config_context[EncodeConfigItemSerial]);
    config_context[EncodeConfigItemSerial].variable_item = serial_item;
    variable_item_set_current_value_text(serial_item, "1234567");

    // Button selection
    VariableItem* button_item = variable_item_list_add(
        variable_item_list, "Button:", COUNT_OF(button_options), 
        protopirate_scene_encode_config_button_callback, &config_context[EncodeConfigItemButton]);
    config_context[EncodeConfigItemButton].variable_item = button_item;
    config_context[EncodeConfigItemButton].options = button_options;
    config_context[EncodeConfigItemButton].option_count = COUNT_OF(button_options);
    config_context[EncodeConfigItemButton].value_index = 0;
    variable_item_set_current_value_index(button_item, 0);
    variable_item_set_current_value_text(button_item, button_options[0]);

    // Counter
    VariableItem* counter_item = variable_item_list_add(
        variable_item_list, "Counter:", 1, 
        protopirate_scene_encode_config_counter_callback, &config_context[EncodeConfigItemCounter]);
    config_context[EncodeConfigItemCounter].variable_item = counter_item;
    variable_item_set_current_value_text(counter_item, "1000");

    // Frequency
    VariableItem* frequency_item = variable_item_list_add(
        variable_item_list, "Frequency:", COUNT_OF(frequency_options), 
        protopirate_scene_encode_config_frequency_callback, &config_context[EncodeConfigItemFrequency]);
    config_context[EncodeConfigItemFrequency].variable_item = frequency_item;
    config_context[EncodeConfigItemFrequency].options = frequency_options;
    config_context[EncodeConfigItemFrequency].option_count = COUNT_OF(frequency_options);
    config_context[EncodeConfigItemFrequency].value_index = 0;
    variable_item_set_current_value_index(frequency_item, 0);
    variable_item_set_current_value_text(frequency_item, frequency_options[0]);

    // Transmit button
    VariableItem* transmit_item = variable_item_list_add(
        variable_item_list, "Transmit Signal", 1, 
        protopirate_scene_encode_config_transmit_callback, &config_context[EncodeConfigItemTransmit]);
    config_context[EncodeConfigItemTransmit].variable_item = transmit_item;
    variable_item_set_current_value_text(transmit_item, "Press OK");

    // Set context for all items
    for(int i = 0; i < EncodeConfigItemCount; i++) {
        if(config_context[i].variable_item) {
            variable_item_set_current_value_index(config_context[i].variable_item, 0);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewVariableItemList);
}

bool protopirate_scene_encode_config_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    ProtoPirateApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == EncodeConfigItemTransmit) {
            // Handle transmission
            notification_message(app->notifications, &sequence_success);
            
            // In a real implementation, this would:
            // 1. Create a FlipperFormat with the selected parameters
            // 2. Initialize the appropriate transmitter
            // 3. Transmit the signal
            
            FURI_LOG_I(TAG, "Transmission triggered with current settings");
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void protopirate_scene_encode_config_on_exit(void* context) {
    furi_assert(context);
    ProtoPirateApp* app = context;
    variable_item_list_reset(app->variable_item_list);
}
