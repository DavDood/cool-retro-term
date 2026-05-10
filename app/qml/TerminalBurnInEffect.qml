import QtQuick 2.2

Item {
    id: root

    property alias effectSource: burnIn.effectSource
    property alias contentSource: burnIn.contentSource
    property QtObject terminalSourceTarget: null

    QtObject {
        id: updateBridge
        signal sourceUpdated()
    }

    Connections {
        target: root.terminalSourceTarget
        ignoreUnknownSignals: true

        onImagePainted: updateBridge.sourceUpdated()
    }

    BurnInEffect {
        id: burnIn
        anchors.fill: parent
        updateTarget: updateBridge
    }
}
