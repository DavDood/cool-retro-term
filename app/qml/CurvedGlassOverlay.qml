/*******************************************************************************
* Copyright (c) 2013-2021 "Filippo Scognamiglio"
* https://github.com/Swordfish90/cool-retro-term
*
* This file is part of cool-retro-term.
*
* cool-retro-term is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*******************************************************************************/

import QtQuick 2.2

ShaderEffect {
    property real screenCurvature: 0.0
    property real frameSize: 0.0
    property real screenRadius: 0.0
    property real windowScaling: 1.0
    property size viewportSize: Qt.size(width / windowScaling, height / windowScaling)

    property real curvedGlass: 0.0
    property real curvedGlassHighlightBurn: 0.50

    blending: true
    visible: curvedGlass > 0

    vertexShader: "qrc:/shaders/curved_glass_overlay.vert.qsb"
    fragmentShader: "qrc:/shaders/curved_glass_overlay.frag.qsb"

    onStatusChanged: if (log) console.log(log)
}
