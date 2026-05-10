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

import "menus"

QtObject {
    id: appRoot

    property ApplicationSettings appSettings: ApplicationSettings {
        onInitializedSettings: appRoot.createWindow()
    }

    property bool renderMode: renderController && renderController.renderMode
    property Loader renderTimeSourceLoader: Loader {
        active: renderMode
        source: active ? "RenderTimeSource.qml" : ""
    }

    property TimeManager timeManager: TimeManager {
        externalTimeSource: renderTimeSourceLoader.item
        hasVisibleWindows: windowsModel.count > 0
        hasContinuousAnimation: appSettings.staticNoise > 0
            || appSettings.glowingLine > 0
            || appSettings.jitter > 0
            || appSettings.horizontalSync > 0
            || appSettings.flickering > 0
    }

    property SettingsWindow settingsWindow: SettingsWindow {
        visible: false
    }

    property AboutDialog aboutDialog: AboutDialog {
        visible: false
    }

    property Component windowComponent: Component {
        TerminalWindow { }
    }

    property ListModel windowsModel: ListModel { }

    property bool initialFullscreenRequested: Qt.application.arguments.indexOf("--fullscreen") !== -1

    function createWindow() {
        if (renderMode) {
            var progressComponent = Qt.createComponent("qrc:/RenderProgressWindow.qml")
            if (progressComponent.status !== Component.Ready) {
                console.log(progressComponent.errorString())
                return
            }

            var progressWindow = progressComponent.createObject(null)
            if (!progressWindow) {
                return
            }

            windowsModel.append({ window: progressWindow })
            initialFullscreenRequested = false
            progressWindow.show()
            progressWindow.requestActivate()
            return
        }

        var window = windowComponent.createObject(null, {
            fullscreen: initialFullscreenRequested
        })
        if (!window)
            return

        windowsModel.append({ window: window })
        initialFullscreenRequested = false
        window.show()
        window.requestActivate()
    }

    function closeWindow(window) {
        for (var i = 0; i < windowsModel.count; i++) {
            if (windowsModel.get(i).window === window) {
                windowsModel.remove(i)
                break
            }
        }

        window.destroy()

        if (windowsModel.count === 0) {
            appSettings.close()
        }
    }
}
