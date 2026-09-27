#include "asn1viewer/ber.hpp"

#include <QApplication>
#include <QFileDialog>
#include <QMainWindow>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <fstream>

using namespace asn1viewer;

static void addNode(QTreeWidgetItem* parent, const std::shared_ptr<Asn1Node>& node) {
    const auto fallbackName = node->tagClass == TagClass::Universal
        ? universalTypeName(node->tagNumber)
        : tagClassName(node->tagClass) + " tag " + std::to_string(node->tagNumber);
    const QString name = QString::fromStdString(node->displayName.empty() ? fallbackName : node->displayName);
    auto* item = new QTreeWidgetItem(parent, {name, QString::fromStdString(tagClassName(node->tagClass)),
                                               QString::number(node->encodedRange.offset), QString::number(node->encodedRange.length),
                                               QString::fromStdString(node->decodedValue)});
    item->setData(0, Qt::UserRole, QString::fromStdString(node->rawHex));
    for (const auto& child : node->children) addNode(item, child);
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QMainWindow window;
    auto* central = new QWidget(&window);
    auto* layout = new QVBoxLayout(central);
    auto* tree = new QTreeWidget(central);
    tree->setHeaderLabels({"Node", "Class", "Offset", "Length", "Value"});
    auto* details = new QPlainTextEdit(central);
    details->setReadOnly(true);
    layout->addWidget(tree); layout->addWidget(details);
    window.setCentralWidget(central);
    window.resize(1000, 700);
    QObject::connect(tree, &QTreeWidget::itemClicked, [&details](QTreeWidgetItem* item) {
        details->setPlainText(item->data(0, Qt::UserRole).toString());
    });
    const auto path = QFileDialog::getOpenFileName(&window, "Open ASN.1 binary");
    if (!path.isEmpty()) {
        std::ifstream file(path.toStdString(), std::ios::binary);
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
        BinaryReader reader(std::move(bytes));
        for (const auto& node : BerDecoder(reader).decodeAll()) addNode(tree->invisibleRootItem(), node);
    }
    window.show();
    return app.exec();
}
