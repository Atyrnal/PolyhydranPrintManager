#include "printersmodel.h"

PrintersModel::PrintersModel(QMap<quint32, Printer*>* printerslist, QObject* parent) : QAbstractListModel(parent), printers(printerslist) {}

QVariant PrintersModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= printers->size()) return {};
    Printer* p = std::next(printers->cbegin(), index.row()).value();
    switch (role) {
    case NameRole: return p->getName();
    case BrandRole: return p->getBrand();
    case ConnectionStatusRole: return p->getConnectionStatus();
    case JobStatusRole: return p->getJobStatus();
    case ModelRole: return p->getModel();
    case PrinterObjectRole: return QVariant::fromValue(p);
    default: return {};
    }
}

QHash<int, QByteArray> PrintersModel::roleNames() const {
    return {
        { NameRole,             "name"             },
        { BrandRole,            "brand"            },
        { ConnectionStatusRole, "connectionStatus" },
        { JobStatusRole,        "jobStatus"        },
        { ModelRole, "model"},
        { PrinterObjectRole,    "printerObject"    },
    };
}