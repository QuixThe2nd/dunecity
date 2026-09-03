/*
 *  This file is part of Dune Legacy.
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Dune Legacy is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Dune Legacy.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <misc/FrameYield.h>

#ifdef __EMSCRIPTEN__

#include <emscripten.h>

void yieldFrameToBrowser() {
    // A zero-length sleep still unwinds the Asyncify call stack to the
    // browser event loop and resumes afterwards: pending signaling and
    // DataChannel callbacks get a chance to run every frame.
    emscripten_sleep(0);
}

#endif // __EMSCRIPTEN__
