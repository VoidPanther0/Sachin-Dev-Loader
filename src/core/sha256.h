#pragma once

#include <string>

namespace sha256 {

// Local-only password hashing for the demo account store. Not a substitute
// for a real server-side auth flow once a backend exists.
std::string hex(const std::string& input);

} // namespace sha256
