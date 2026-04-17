/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/

import QtQuick
import QtQuick.Effects
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts

//UI declarations

ApplicationWindow { //Root app window
    id: rootWindow
    visible: true
    width: 800
    height: 600
    title: qsTr("Polyhydran Print Manager Configuration Utility")
    //visibility: Window.FullScreen //Make fullscreen

    enum AppState {
        Welcome,
        Config
    }

    Material.theme : Material.Dark;

    property int appstate: Main.AppState.Welcome; //Track app state
}
