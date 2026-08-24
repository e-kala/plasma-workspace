/*
    SPDX-FileCopyrightText: 2013 Sebastian Kügler <sebas@kde.org>
    SPDX-FileCopyrightText: 2014 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2019-2020 Konrad Materka <materka@gmail.com>
    SPDX-FileCopyrightText: 2022 ivan (@ratijas) tkachenko <me@ratijas.tk>
    SPDX-FileCopyrightText: 2025 Kristen McWilliam <kristen@kde.org>
    SPDX-FileCopyrightText: 2025 Nate Graham <nate@kde.org>
    SPDX-FileCopyrightText: 2026 Nathaniel Krebs <areyoufeelingitnowmrkrebs@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kcmutils as KCMUtils
import org.kde.kirigami as Kirigami
import org.kde.kquickcontrols as KQC
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid

KCMUtils.ScrollViewKCM {
    id: iconsPage

    signal configurationChanged
    property bool unsavedChanges: !(changedVisibility.size === 0 && changedShortcuts.size === 0)

    property bool cfg_scaleIconsToFit
    property int cfg_iconSpacing
    property bool cfg_reverseIconOrder
    property bool cfg_showAllItems
    property list<string> cfg_shownItems: []
    property list<string> cfg_hiddenItems: []
    property list<string> cfg_extraItems: []
    property list<string> cfg_disabledStatusNotifiers: []
    property list<string> cfg_iconOrder: []

    property string searchFilterText: ""

    // We can share one combobox model across all delegates, they all have the same options
    readonly property var comboBoxModel: {
        const autoElement = {
            "value": "auto",
            "text": i18nc("@item:inlistbox Show this System Tray item when relevant", "Show when relevant")
        };
        const shownElement = {
            "value": "shown",
            "text": i18nc("@item:inlistbox Always show this System Tray item", "Always show")
        };
        const hiddenElement = {
            "value": "hidden",
            "text": i18nc("@item:inlistbox Show this System Tray item only in the expanded popup", "Show only in popup")
        };
        const disabledElement = {
            "value": "disabled",
            "text": i18nc("@item:inlistbox Never show this System Tray item; disable it", "Never show (disabled)")
        };

        return cfg_showAllItems ? [autoElement, disabledElement] : [autoElement, shownElement, hiddenElement, disabledElement];
    }

    readonly property var changedShortcuts: new Map()
    readonly property var changedVisibility: new Map()

    ListModel {
        id: orderModel
    }

    readonly property int itemTypeRole: Qt.UserRole + 1
    readonly property int itemIdRole: Qt.UserRole + 2
    readonly property int categoryRole: Qt.UserRole + 4
    readonly property int appletRole: Qt.UserRole + 8
    readonly property int dataEngineSourceRole: Qt.UserRole + 107

    function rowStableKey(modelIndex) {
        const source = modelIndex.data(dataEngineSourceRole);
        return source ? modelIndex.data(itemIdRole) + " " + source : modelIndex.data(itemIdRole);
    }

    function findConfigModelIndexByStableKey(stableKey) {
        const source = Plasmoid.configSystemTrayModel;
        for (let i = 0; i < source.rowCount(); i++) {
            const modelIndex = source.index(i, 0);
            if (rowStableKey(modelIndex) === stableKey) {
                return modelIndex;
            }
        }
        return null;
    }

    function findConfigModelIndex(itemId) {
        const source = Plasmoid.configSystemTrayModel;
        for (let i = 0; i < source.rowCount(); i++) {
            const modelIndex = source.index(i, 0);
            if (modelIndex.data(itemIdRole) === itemId) {
                return modelIndex;
            }
        }
        return null;
    }

    function configDataForItemId(itemId) {
        const modelIndex = findConfigModelIndex(itemId);
        if (!modelIndex) {
            return null;
        }
        return {
            "applet": modelIndex.data(appletRole),
            "decoration": modelIndex.data(Qt.DecorationRole),
        };
    }

    function orderModelRowData(modelIndex) {
        return {
            "itemId": modelIndex.data(itemIdRole),
            "stableKey": rowStableKey(modelIndex),
            "displayText": modelIndex.data(Qt.DisplayRole),
            "itemType": modelIndex.data(itemTypeRole),
            "category": modelIndex.data(categoryRole),
        };
    }

    function initOrderModel() {
        const source = Plasmoid.configSystemTrayModel;
        orderModel.clear();
        for (let i = 0; i < source.rowCount(); i++) {
            const modelIndex = source.index(i, 0);
            if (modelIndex.data(itemIdRole)) {
                orderModel.append(orderModelRowData(modelIndex));
            }
        }
    }

    readonly property var categoryOrder: ["UnknownCategory", "ApplicationStatus", "Communications", "SystemServices", "Hardware"]

    function categoryOrderIndex(category) {
        const index = categoryOrder.indexOf(category);
        return index === -1 ? categoryOrder.indexOf("UnknownCategory") : index;
    }

    function restoreDefaultOrder() {
        const targetOrder = [];
        for (let i = 0; i < orderModel.count; i++) {
            const row = orderModel.get(i);
            targetOrder.push({
                "itemId": row.itemId,
                "displayText": row.displayText,
                "category": row.category,
            });
        }
        targetOrder.sort((a, b) => {
            const categoryDiff = categoryOrderIndex(a.category) - categoryOrderIndex(b.category);
            if (categoryDiff !== 0) {
                return categoryDiff;
            }
            return a.displayText.localeCompare(b.displayText);
        });

        for (let i = 0; i < targetOrder.length; i++) {
            let currentIndex = -1;
            for (let j = i; j < orderModel.count; j++) {
                if (orderModel.get(j).itemId === targetOrder[i].itemId) {
                    currentIndex = j;
                    break;
                }
            }
            if (currentIndex !== -1 && currentIndex !== i) {
                orderModel.move(currentIndex, i, 1);
            }
        }

        cfg_iconOrder = [];
    }

    function syncOrderModel() {
        const source = Plasmoid.configSystemTrayModel;

        const liveKeys = [];
        for (let i = 0; i < source.rowCount(); i++) {
            const modelIndex = source.index(i, 0);
            if (modelIndex.data(itemIdRole)) {
                liveKeys.push(rowStableKey(modelIndex));
            }
        }

        for (let i = orderModel.count - 1; i >= 0; i--) {
            if (liveKeys.indexOf(orderModel.get(i).stableKey) === -1) {
                orderModel.remove(i);
            }
        }

        const knownKeys = [];
        for (let i = 0; i < orderModel.count; i++) {
            knownKeys.push(orderModel.get(i).stableKey);
        }

        for (let i = source.rowCount() - 1; i >= 0; i--) {
            const modelIndex = source.index(i, 0);
            if (!modelIndex.data(itemIdRole)) {
                continue;
            }
            const stableKey = rowStableKey(modelIndex);
            if (knownKeys.indexOf(stableKey) !== -1) {
                continue;
            }

            let insertIndex = orderModel.count;
            for (let j = i + 1; j < source.rowCount(); j++) {
                const anchorIndex = knownKeys.indexOf(rowStableKey(source.index(j, 0)));
                if (anchorIndex !== -1) {
                    insertIndex = anchorIndex;
                    break;
                }
            }

            orderModel.insert(insertIndex, orderModelRowData(modelIndex));
            knownKeys.splice(insertIndex, 0, stableKey);
        }
    }

    function refreshOrderModelRow(stableKey) {
        for (let i = 0; i < orderModel.count; i++) {
            if (orderModel.get(i).stableKey === stableKey) {
                const modelIndex = findConfigModelIndexByStableKey(stableKey);
                if (modelIndex) {
                    orderModel.set(i, orderModelRowData(modelIndex));
                }
                return true;
            }
        }
        return false;
    }

    function moveOrderModelEntry(oldIndex, newIndex) {
        if (oldIndex === newIndex || oldIndex < 0 || newIndex < 0 || oldIndex >= orderModel.count || newIndex >= orderModel.count) {
            return;
        }
        orderModel.move(oldIndex, newIndex, 1);

        const order = [];
        for (let i = 0; i < orderModel.count; i++) {
            order.push(orderModel.get(i).itemId);
        }
        cfg_iconOrder = order;
    }

    Component.onCompleted: initOrderModel()

    function saveConfig() {
        for (const [key, value] of changedVisibility.entries()) {
            updateVisibility(key, value);
        }
        for (const [key, value] of changedShortcuts.entries()) {
            key.globalShortcut = value;
        }
        changedShortcuts.clear();
        changedVisibility.clear();
        cfg_shownItemsChanged();
        cfg_hiddenItemsChanged();
        cfg_extraItemsChanged();
        cfg_disabledStatusNotifiersChanged();
        changedShortcutsChanged();
        changedVisibilityChanged();
    }

    function updateVisibility(itemId: string, visibility: string): void {
        const shownIndex = cfg_shownItems.indexOf(itemId);
        const hiddenIndex = cfg_hiddenItems.indexOf(itemId);
        const extraIndex = cfg_extraItems.indexOf(itemId);
        const disabledSniIndex = cfg_disabledStatusNotifiers.indexOf(itemId);

        if (shownIndex > -1) {
            cfg_shownItems.splice(shownIndex, 1);
        }
        if (hiddenIndex > -1) {
            cfg_hiddenItems.splice(hiddenIndex, 1);
        }
        if (extraIndex > -1) {
            cfg_extraItems.splice(extraIndex, 1);
        }
        if (disabledSniIndex > -1) {
            cfg_disabledStatusNotifiers.splice(disabledSniIndex, 1);
        }

        switch (visibility) {
        case "auto":
        case "auto-sni":
            cfg_extraItems.push(itemId);
            break;
        case "shown":
            cfg_extraItems.push(itemId);
        /* fallthrough*/
        case "shown-sni":
            cfg_shownItems.push(itemId);
            break;
        case "hidden":
            cfg_extraItems.push(itemId);
        /* fallthrough */
        case "hidden-sni":
            cfg_hiddenItems.push(itemId);
            break;
        case "disabled-sni":
            cfg_disabledStatusNotifiers.push(itemId);
            break;
        }
    }

    function categoryName(category) {
        switch (category) {
        case "ApplicationStatus":
            return i18n("Application Status");
        case "Communications":
            return i18n("Communications");
        case "SystemServices":
            return i18n("System Services");
        case "Hardware":
            return i18n("Hardware Control");
        case "UnknownCategory":
        default:
            return i18n("Miscellaneous");
        }
    }

    Connections {
        target: Plasmoid.configSystemTrayModel
        function onRowsRemoved() {
            // if the user closes an app with a SNI that has a visibility change queued, we need to remove it
            // from the queue as the change is no longer visible in the UI.
            let idsToRemove = [...iconsPage.changedVisibility.keys()];
            for (let i = 0; i < Plasmoid.configSystemTrayModel.rowCount() && idsToRemove.length > 0; ++i) {
                let itemId = Plasmoid.configSystemTrayModel.index(i, 0).data(Qt.UserRole + 2);
                idsToRemove = idsToRemove.filter(id => id !== itemId);
            }
            idsToRemove.forEach(id => {
                iconsPage.changedVisibility.delete(id);
            });
            if (idsToRemove.length > 0) {
                iconsPage.changedVisibilityChanged();
            }

            iconsPage.syncOrderModel();
        }
        function onRowsInserted() {
            iconsPage.syncOrderModel();
        }
        function onDataChanged(topLeft, bottomRight) {
            let needsSync = false;
            for (let i = topLeft.row; i <= bottomRight.row; i++) {
                const modelIndex = Plasmoid.configSystemTrayModel.index(i, 0);
                if (!modelIndex.data(iconsPage.itemIdRole)) {
                    continue;
                }
                if (!iconsPage.refreshOrderModelRow(iconsPage.rowStableKey(modelIndex))) {
                    needsSync = true;
                }
            }
            if (needsSync) {
                iconsPage.syncOrderModel();
            }
        }
    }

    header: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing

        Kirigami.InlineMessage {
            id: disablingSniMessage
            property string appName

            function showWithAppName(appName: string) {
                disablingSniMessage.appName = appName;
                visible = true;
            }

            Layout.fillWidth: true
            type: Kirigami.MessageType.Warning
            text: xi18nc("@info:usagetip", "Look for a setting in <application>%1</application> to disable its tray icon before doing it here. Some apps’ tray icons were not designed to be disabled, and using this setting may cause them to behave unexpectedly.<nl/><nl/>Use this setting at your own risk, and do not report issues to KDE or the app’s author.", appName)
            actions: [
                Kirigami.Action {
                    text: i18nc("@action:button", "I understand the risks")
                    onTriggered: disablingSniMessage.visible = false
                }
            ]
        }

        Kirigami.InlineMessage {
            id: disablingKlipperMessage

            visible: iconsPage.changedVisibility.get("org.kde.plasma.clipboard") === "disabled"
            Layout.fillWidth: true
            type: Kirigami.MessageType.Warning
            text: xi18nc("@info:usagetip", "Disabling the clipboard is not recommended, as it will cause copied data to be lost when the application it was copied from is closed.<nl/><nl/>Only do this if you’ve manually added a standalone Clipboard widget somewhere else, or are using a 3rd-party clipboard manager. Also, instead consider configuring the clipboard to not save history, or to only remember one item at a time.")
        }

        Kirigami.InlineMessage {
            id: disablingNotificationsMessage

            visible: iconsPage.changedVisibility.get("org.kde.plasma.notifications") === "disabled"
            Layout.fillWidth: true
            type: Kirigami.MessageType.Warning
            text: xi18nc("@info:usagetip", "Disabling the Notifications widget is not recommended, as notifications sent by apps will no longer be shown.<nl/><nl/>Only do this if you’ve manually added a standalone Notifications widget somewhere else, or are intentionally using a 3rd-party notification manager to handle notifications.")
        }

        Kirigami.FormLayout {
            id: formLayout

            readonly property int maxComboboxWidth: Math.max(sizeChooser.implicitWidth, spacingChooser.implicitWidth, layoutChooser.implicitWidth)

            QQC2.ComboBox {
                id: sizeChooser

                readonly property string scaleString: Plasmoid.formFactor === PlasmaCore.Types.Horizontal
                    ? i18nc("@item:inlistbox Icon size", "Scale with Panel height")
                    : i18nc("@item:inlistbox Icon size", "Scale with Panel width")

                Kirigami.FormData.label: i18nc("@label:listbox Whether the system tray icons in the Panel always stay small or scale with the Panel's size", "Panel icon size:")
                Layout.preferredWidth: formLayout.maxComboboxWidth
                model: [
                    {
                        "label": i18nc("@item:inlistbox Icon size", "Small"),
                        "size": "small"
                    },
                    {
                        "label": scaleString,
                        "size": "scale"
                    }
                ]
                textRole: "label"

                currentIndex: iconsPage.cfg_scaleIconsToFit ? 1 : 0

                onActivated: index => {
                    iconsPage.cfg_scaleIconsToFit = model[currentIndex]["size"] == "scale";
                }
            }

            QQC2.ComboBox {
                id: spacingChooser

                Kirigami.FormData.label: i18nc("@label:listbox The spacing between system tray icons in the Panel", "Spacing:")
                Layout.preferredWidth: formLayout.maxComboboxWidth
                model: [
                    {
                        "label": i18nc("@item:inlistbox Icon spacing", "Small"),
                        "spacing": 1
                    },
                    {
                        "label": i18nc("@item:inlistbox Icon spacing", "Normal"),
                        "spacing": 2
                    },
                    {
                        "label": i18nc("@item:inlistbox Icon spacing", "Large"),
                        "spacing": 6
                    }
                ]
                textRole: "label"

                currentIndex: switch (iconsPage.cfg_iconSpacing) {
                    case 1: return 0; // Small
                    case 2: return 1; // Normal
                    case 6: return 2; // Large
                }

                onActivated: index => {
                    iconsPage.cfg_iconSpacing = model[currentIndex]["spacing"];
                }
            }

            QQC2.ComboBox {
                id: layoutChooser

                Kirigami.FormData.label: i18nc("@label:listbox Which direction system tray icons in the Panel emerge from the expander arrow", "Direction:")
                Layout.preferredWidth: formLayout.maxComboboxWidth

                textRole: "text"
                valueRole: "value"

                // Evaluate current system state for dynamic labels
                readonly property bool isRtl: Qt.application.layoutDirection === Qt.RightToLeft
                readonly property bool isVertical: Plasmoid.formFactor === PlasmaCore.Types.Vertical

                model: {
                    if (isVertical) {
                        // Vertical default is arrow at bottom, expanding upwards
                        return [
                            { "text": i18n("Bottom-to-top"), "value": false },
                            { "text": i18n("Top-to-bottom"), "value": true }
                        ];
                    } else if (isRtl) {
                        // RTL default is arrow on left, expanding to the right
                        return [
                            { "text": i18n("Left-to-right"), "value": false },
                            { "text": i18n("Right-to-left"), "value": true }
                        ];
                    } else {
                        // Standard LTR default is arrow on right, expanding to the left
                        return [
                            { "text": i18n("Right-to-left"), "value": false },
                            { "text": i18n("Left-to-right"), "value": true }
                        ];
                    }
                }

                currentIndex: iconsPage.cfg_reverseIconOrder ? 1 : 0
                onActivated: iconsPage.cfg_reverseIconOrder = currentValue;
            }

            QQC2.Label {
                visible: Kirigami.Settings.tabletMode
                text: i18nc("@info:usagetip under a combobox when Touch Mode is on", "Automatically set to Large when in Touch Mode")
                textFormat: Text.PlainText
                font: Kirigami.Theme.smallFont
            }
        }
    }

    view: ListView {
        id: itemsList

        property real keySequenceColumnWidth: Kirigami.Units.gridUnit
        readonly property int iconSize: Kirigami.Units.iconSizes.smallMedium

        clip: true

        model: orderModel
        reuseItems: true

        headerPositioning: ListView.OverlayHeader
        header: Kirigami.InlineViewHeader {
            width: itemsList.width
            text: i18nc("@title:column", "Entries")
            actions: [
                Kirigami.Action {
                    id: showAllCheckBox
                    text: i18nc("@option:check, This is referring to system tray entries", "Always show all")
                    onToggled: iconsPage.cfg_showAllItems = checked
                    checked: iconsPage.cfg_showAllItems
                    displayComponent: QQC2.CheckBox {
                        horizontalPadding: Kirigami.Units.largeSpacing
                        text: showAllCheckBox.text
                        checked: showAllCheckBox.checked
                        onToggled: showAllCheckBox.toggle()
                    }
                },
                Kirigami.Action {
                    id: searchAction
                    displayComponent: Kirigami.SearchField {
                        onTextChanged: {
                            // reset currentIndex to avoid passing focus to the currentItem when
                            // the model updates
                            itemsList.currentIndex = -1
                            iconsPage.searchFilterText = text
                        }
                    }
                },
                Kirigami.Action {
                    icon.name: "edit-undo"
                    text: i18nc("@action:button Reset the manually dragged order of system tray entries back to the automatic, category-based order", "Restore default order") // qmllint disable unqualified
                    enabled: iconsPage.cfg_iconOrder.length > 0
                    onTriggered: iconsPage.restoreDefaultOrder()
                }
            ]
        }

        moveDisplaced: Transition {
            YAnimator {
                duration: Kirigami.Units.longDuration
                easing.type: Easing.InOutCubic
            }
        }

        delegate: Item {
            id: delegateRoot

            required property string category
            required property string displayText
            required property int index
            required property string itemId
            required property string itemType

            readonly property var configData: iconsPage.configDataForItemId(itemId)
            readonly property var applet: configData ? configData.applet : null
            readonly property var decoration: configData ? configData.decoration : undefined

            readonly property bool matchesFilter: !iconsPage.searchFilterText || displayText.toLowerCase().indexOf(iconsPage.searchFilterText.toLowerCase()) !== -1
            visible: matchesFilter
            width: itemsList.width
            implicitHeight: trayItemDelegate.implicitHeight
            height: visible ? implicitHeight : 0

            TrayItemDelegate {
                id: trayItemDelegate
                anchors.left: parent.left
                anchors.right: parent.right
                applet: delegateRoot.applet
                category: delegateRoot.category
                decoration: delegateRoot.decoration
                displayText: delegateRoot.displayText
                index: delegateRoot.index
                itemId: delegateRoot.itemId
                itemType: delegateRoot.itemType
            }
        }
    }

    // Re-add separator line between footer and list view
    extraFooterTopPadding: true

    component ReorderButton: QQC2.ToolButton {
        id: reorderButton

        required property string accessibleText

        display: QQC2.AbstractButton.IconOnly
        icon.width: Kirigami.Units.iconSizes.small
        icon.height: Kirigami.Units.iconSizes.small
        implicitWidth: Kirigami.Units.iconSizes.small + topPadding + bottomPadding
        implicitHeight: implicitWidth

        Accessible.name: accessibleText
        QQC2.ToolTip.text: accessibleText
        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
    }

    /*!
    * Delegate representing each system tray item.
    */
    component TrayItemDelegate: QQC2.ItemDelegate {
        id: listItem

        required property var applet
        required property string category
        required property var decoration
        required property string displayText
        required property int index
        required property string itemId
        required property string itemType

        Kirigami.Theme.useAlternateBackgroundColor: true

        // Don't need highlight, hover, or pressed effects
        highlighted: false
        hoverEnabled: false
        down: false

        readonly property bool isPlasmoid: itemType === "Plasmoid"

        contentItem: FocusScope {
            implicitHeight: childrenRect.height

            onActiveFocusChanged: if (activeFocus) {
                listItem.ListView.view.positionViewAtIndex(listItem.index, ListView.Contain);
            }

            RowLayout {
                width: parent.width
                spacing: Kirigami.Units.smallSpacing

                Kirigami.ListItemDragHandle {
                    listItem: listItem
                    listView: itemsList
                    enabled: iconsPage.searchFilterText.length === 0
                    visible: enabled

                    onMoveRequested: (oldIndex, newIndex) => iconsPage.moveOrderModelEntry(oldIndex, newIndex)
                }

                ColumnLayout {
                    spacing: 0
                    visible: iconsPage.searchFilterText.length === 0

                    ReorderButton {
                        icon.name: "go-up-symbolic"
                        enabled: listItem.index > 0
                        accessibleText: i18nc("@action:button", "Move entry up")
                        onClicked: iconsPage.moveOrderModelEntry(listItem.index, listItem.index - 1)
                    }

                    ReorderButton {
                        icon.name: "go-down-symbolic"
                        enabled: listItem.index < orderModel.count - 1
                        accessibleText: i18nc("@action:button", "Move entry down")
                        onClicked: iconsPage.moveOrderModelEntry(listItem.index, listItem.index + 1)
                    }
                }

                Kirigami.Icon {
                    implicitWidth: itemsList.iconSize
                    implicitHeight: itemsList.iconSize
                    source: listItem.decoration
                    animated: false
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    QQC2.Label {
                        id: nameLabel
                        Layout.fillWidth: true
                        maximumLineCount: 1
                        text: listItem.displayText
                        textFormat: Text.PlainText
                        elide: Text.ElideRight

                        QQC2.ToolTip {
                            visible: listItem.hovered && nameLabel.truncated
                            text: nameLabel.text
                        }
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        maximumLineCount: 1
                        text: iconsPage.categoryName(listItem.category)
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        font: Kirigami.Theme.smallFont
                        color: Kirigami.Theme.disabledTextColor
                    }
                }

                QQC2.ComboBox {
                    id: visibilityComboBox

                    readonly property string currentVisibility: iconsPage.changedVisibility.has(listItem.itemId) ? iconsPage.changedVisibility.get(listItem.itemId).replace("-sni", "") : originalVisibility
                    readonly property string originalVisibility: {
                        if (iconsPage.cfg_showAllItems || iconsPage.cfg_shownItems.indexOf(listItem.itemId) !== -1) {
                            return "shown";
                        } else if (iconsPage.cfg_hiddenItems.indexOf(listItem.itemId) !== -1) {
                            return "hidden";
                        } else if ((listItem.isPlasmoid && iconsPage.cfg_extraItems.indexOf(listItem.itemId) === -1) || (!listItem.isPlasmoid && iconsPage.cfg_disabledStatusNotifiers.indexOf(listItem.itemId) > -1)) {
                            return "disabled";
                        } else {
                            return "auto";
                        }
                    }

                    implicitContentWidthPolicy: QQC2.ComboBox.WidestText

                    enabled: !iconsPage.cfg_showAllItems && listItem.itemId
                    textRole: "text"
                    valueRole: "value"

                    currentIndex: {
                        for (let i = 0; i < model.length; i++) {
                            if (model[i].value === currentVisibility) {
                                return i;
                            }
                        }

                        return 0;
                    }
                    model: iconsPage.comboBoxModel

                    onActivated: index => {
                        if (currentValue !== originalVisibility) {
                            if (currentValue === "disabled" && !listItem.isPlasmoid) {
                                disablingSniMessage.showWithAppName(nameLabel.text);
                            }
                            iconsPage.changedVisibility.set(listItem.itemId, currentValue + (listItem.isPlasmoid ? "" : "-sni"));
                        } else {
                            iconsPage.changedVisibility.delete(listItem.itemId);
                        }
                        iconsPage.changedVisibilityChanged();
                    }
                }

                KQC.KeySequenceItem {
                    id: keySequenceItem
                    Layout.minimumWidth: itemsList.keySequenceColumnWidth
                    Layout.preferredWidth: itemsList.keySequenceColumnWidth

                    // We want to keep the column the same size for all items,
                    // but only when not inputting a new shortcut, as the length
                    // of the shortcut may vary quite a bit while inputting. So
                    // rather than binding to widthChanged, we update the maximum
                    // column width only at specific moments: When the item is
                    // first added, or when we know the shortcut has changed and
                    // no further input happens.
                    function updateColumnWidth() {
                        itemsList.keySequenceColumnWidth = Math.max(implicitWidth, itemsList.keySequenceColumnWidth);
                    }

                    // Delay this one tick to make sure the item has finished
                    // any layout that still needs to be done.
                    Component.onCompleted: Qt.callLater(updateColumnWidth)

                    visible: listItem.isPlasmoid
                    enabled: visibilityComboBox.currentValue !== "disabled"
                    readonly property string originalKeySequence: listItem.applet ? listItem.applet.plasmoid.globalShortcut : ""
                    keySequence: iconsPage.changedShortcuts.has(listItem.applet?.plasmoid) ? iconsPage.changedShortcuts.get(listItem.applet?.plasmoid) : originalKeySequence

                    onCaptureFinished: {
                        if (listItem.applet) {
                            if (keySequence !== listItem.applet.plasmoid.globalShortcut) {
                                iconsPage.changedShortcuts.set(listItem.applet.plasmoid, keySequence.toString());
                            } else {
                                iconsPage.changedShortcuts.delete(listItem.applet.plasmoid);
                            }
                            iconsPage.changedShortcutsChanged();

                            updateColumnWidth()
                        }
                    }
                }
                // Placeholder for when KeySequenceItem is not visible
                Item {
                    Layout.minimumWidth: itemsList.keySequenceColumnWidth
                    Layout.maximumWidth: itemsList.keySequenceColumnWidth
                    visible: !keySequenceItem.visible
                }

                QQC2.Button {
                    readonly property QtObject configureAction: (listItem.applet && listItem.applet.plasmoid.internalAction("configure")) || null

                    Accessible.name: configureAction ? configureAction.text : ""
                    icon.name: "configure"
                    enabled: configureAction && configureAction.visible && configureAction.enabled
                    // Still reserve layout space, so not setting visible to false
                    opacity: enabled ? 1 : 0
                    onClicked: configureAction.trigger()

                    QQC2.ToolTip {
                        // Strip out ampersands right before non-whitespace characters, i.e.
                        // those used to determine the alt key shortcut
                        text: parent.Accessible.name.replace(/&(?=\S)/g, "")
                    }
                }
            }
        }
    }
}
