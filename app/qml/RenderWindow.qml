import QtQuick 2.2
import QtQuick.Window 2.2

Window {
    id: terminalWindow

    width: Math.max(1, renderController.frameWidth)
    height: Math.max(1, renderController.frameHeight)
    visible: true
    color: "transparent"
    title: ""
    flags: Qt.Tool | Qt.FramelessWindowHint
    opacity: 0.0
    x: -width - 100
    y: -height - 100

    property bool fullscreen: false
    property real normalizedWindowScale: 1024 / ((0.5 * width + 0.5 * height))

    onFullscreenChanged: visibility = (fullscreen ? Window.FullScreen : Window.Windowed)

    Component.onCompleted: renderController.start()

    RenderFrameView {
        id: frameRenderer
        anchors.fill: parent
    }

    onClosing: appRoot.closeWindow(terminalWindow)
}
