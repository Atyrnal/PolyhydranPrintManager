#ifndef PRINTERSMODEL_H
#define PRINTERSMODEL_H

#include <QAbstractListModel>
#include <QList>
#include "printer.h"

class PrintersModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        BrandRole,
        ConnectionStatusRole,
        JobStatusRole,
        ModelRole,
        PrinterObjectRole,
    };
    explicit PrintersModel(QMap<quint32, Printer*>* printers, QObject* parent = nullptr);

    int rowCount(const QModelIndex &parent ={}) const override { return printers->size(); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
private:
    QMap<quint32, Printer*>* printers;
};

#endif // PRINTERSMODEL_H
