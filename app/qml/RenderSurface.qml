import QtQuick 2.2

Item {
    id: renderSurface

    width: Math.max(1, renderController.frameWidth)
    height: Math.max(1, renderController.frameHeight)
    visible: true

    property real normalizedWindowScale: 1024 / ((0.5 * width + 0.5 * height))

    RenderFrameView {
        id: frameRenderer
        anchors.fill: parent
        normalizedWindowScale: renderSurface.normalizedWindowScale
    }
}
