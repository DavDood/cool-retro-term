import QtQuick 2.2

Item {
    id: root

    signal sourceUpdated()

    property alias effectSource: burnIn.effectSource
    property alias contentSource: burnIn.contentSource
    property alias lastUpdate: burnIn.lastUpdate
    property alias burnInFadeTime: burnIn.burnInFadeTime

    BurnInEffect {
        id: burnIn
        anchors.fill: parent
        updateTarget: root
    }
}
