pragma Singleton
import QtQuick 2.15

QtObject {
    function missing(family, text) {
        return [];
    }

    function missingDisplay(family, text) {
        return [];
    }
}
