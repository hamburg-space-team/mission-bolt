/**
 * THHOR-BOLT REXUS 37 - Downlink Packet Protocol
 *
 * Wire format: [Header 12B] [Payload ≤50B] [CRC16 2B] = 64B max
 */

#pragma once

#include "packet_header.hpp"
#include "packet_payloads.hpp"
#include "packet_types.hpp"