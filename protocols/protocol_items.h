// protocols/protocol_items.h
#pragma once

#include <lib/subghz/types.h>

// KIA/Hyundai protocols (already in project)
#include "kia_generic.h"
#include "kia_v0.h"
#include "kia_v1.h"
#include "kia_v2.h"
#include "kia_v3_v4.h"
#include "kia_v5.h"
#include "hyundai_v0.h"

// Asian manufacturers
#include "ford_v0.h"
#include "subaru.h"
#include "suzuki.h"
#include "honda_v0.h"
#include "honda_v2.h"

// European manufacturers - VAG Group
#include "vw.h"

// European manufacturers - PSA Group
#include "citroen.h"

// European manufacturers - Fiat/Japanese
#include "fiat_v0.h"

// American manufacturers
// Note: tesla.h is not implemented yet

extern const SubGhzProtocolRegistry protopirate_protocol_registry;
