#pragma once
#ifndef THEATRE_AC_LEGACY_082
#define THEATRE_AC_LEGACY_082 0
#endif
static_assert(THEATRE_AC_LEGACY_082==0||THEATRE_AC_LEGACY_082==1,"Invalid AC profile");
namespace AcProfile {
constexpr bool LEGACY=THEATRE_AC_LEGACY_082;
constexpr bool PULSE_CONFIGURABLE=!LEGACY;
constexpr unsigned FIXED_PULSE_US=200;
constexpr unsigned MAX_PULSE_US=LEGACY?200:400;
constexpr const char *NAME=LEGACY?"legacy082-fixed200-stop-safe":"adjustable087";
}
