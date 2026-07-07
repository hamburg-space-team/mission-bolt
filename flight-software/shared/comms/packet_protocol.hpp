/// THHOR-BOLT REXUS 37 - umbrella include for the downlink packet
/// protocol. Wire format: [Header 12B] [Payload <= 50B] [CRC16 2B] = 64B max.
///
/// @ingroup comms

#pragma once

#include "packet_header.hpp"   // IWYU pragma: keep
#include "packet_payloads.hpp" // IWYU pragma: keep
#include "packet_types.hpp"    // IWYU pragma: keep
