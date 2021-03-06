/**
 *   Copyright 2016, Quickstep Research Group, Computer Sciences Department,
 *     University of Wisconsin—Madison.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 **/

#include "AccessMode.hpp"

namespace quickstep {
namespace transaction {

const bool AccessMode::kLockCompatibilityMatrix[kNumberLocks][kNumberLocks] = {
/*           NL     IS     IX      S     SIX     X    */
/*  NL  */ {true , true , true , true , true , true },
/*  IS  */ {true , true , true , true , true , false},
/*  IX  */ {true , true , true , false, false, false},
/*  S   */ {true , true , false, true , false, false},
/*  SIX */ {true , true , false, false, false, false},
/*  X   */ {true , false, false, false, false, false}
};

}  // namespace transaction
}  // namespace quickstep
