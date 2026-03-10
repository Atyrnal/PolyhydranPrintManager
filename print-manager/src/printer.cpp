/*
 *
 * Copyright (c) 2025 Antony Rinaldi
 *
*/


#include "printer.h"

Printer::Printer(QObject* parent) : QObject(parent) {}

Printer::Printer(QString name, QString model, QObject* parent) : Printer(parent) {
    this->name = name;
    this->model = model;
}

Printer::Printer(QString name, QString model, QString brand, QObject* parent) : Printer(parent) {
    this->name = name;
    this->model = model;
    this->brand = brand;
}

void Printer::setName(QString name) {
    this->name = name;
}

void Printer::setModel(QString model) {
    this->model = model;
}

void Printer::setBrand(QString brand) {
    this->brand = brand;
}

QString Printer::getName() {
    return this->name;
}

QString Printer::getModel() {
    return this->model;
}

QString Printer::getBrand() {
    return this->brand;
}
