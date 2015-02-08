#include "wndmain.h"
#include "ui_wndmain.h"
#include <QDebug>
#include <QMessageBox>

WndMain::WndMain(QWidget *parent) : QMainWindow(parent), _ui(new Ui::WndMain) {
    _ui->setupUi(this);

    _serial = new QSerialPort();

    _liveDataTimer = new QTimer();
    _liveDataTimer->setSingleShot(false);
    _liveDataTimer->setInterval(50);
    connect(_liveDataTimer, SIGNAL(timeout()), this, SLOT(_updateLiveData()));

    _connectTimer = new QTimer();
    _connectTimer->setSingleShot(true);
    _connectTimer->setInterval(1000);
    connect(_connectTimer, SIGNAL(timeout()), this, SLOT(_scanPorts()));

    connect(_ui->pbReadEcuMap, SIGNAL(clicked()), this, SLOT(_readEcuMap()));
    connect(_ui->pbWriteEcuMap, SIGNAL(clicked()), this, SLOT(_writeEcuMap()));

    connect(_ui->pbReadParams, SIGNAL(clicked()), this, SLOT(_readParams()));
    connect(_ui->pbWriteParams, SIGNAL(clicked()), this, SLOT(_writeParams()));
    _ecuDisconnected();
}

WndMain::~WndMain() {
    delete _connectTimer;
    delete _serial;
    delete _ui;
}

void WndMain::changeEvent(QEvent *e) {
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        _ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

bool WndMain::_ecuConnect(const QSerialPortInfo &port) {
    QByteArray version;

    _serial->setPort(port);
    _serial->setBaudRate(9600);
    if (!_serial->open(QIODevice::ReadWrite)) {
        return false;
    }

    _ui->statusBar->showMessage(QString::fromUtf8("Port %1 otwarty...").arg(port.portName()));


    /* Próbujemy odczytać wersje softu */
    if (!_ecuCommand(QByteArray("v\r\n"), NULL, &version))
        return false;

    _ui->statusBar->showMessage(QString::fromUtf8("Podłączono do: %1 (port: %2)").arg(QString(version.trimmed())).arg(port.portName()));

    _readEcuMap();
    _readParams();

    _liveDataTimer->start();

    return true;
}

void WndMain::_ecuDisconnected() {
    if (_serial->isOpen())
        _serial->close();

    _liveDataTimer->stop();

    _ui->lCrankAccel->setText("-");
    _ui->lEngineTemp->setText(QString::fromUtf8("- °C"));
    _ui->lIgnitionAdvance->setText(QString::fromUtf8("- °"));
    _ui->lRPM->setText("- RPM");

    _ui->statusBar->showMessage(QString::fromUtf8("Oczekiwanie na połączenie do ECU..."));

    _connectTimer->start();
}

void WndMain::_scanPorts() {
    static int num = 0;
    qDebug() << "num = " << ++num;

    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        if (info.description() == "MZ ETZ ECU") {
            _ui->statusBar->showMessage(QString::fromUtf8("Znaleziono ECU na porcie: %1, próba połączenia...").arg(info.portName()));
            if (_ecuConnect(info))
                return;
        }
    }

    _connectTimer->start();
}

void WndMain::_updateLiveData() {
    QByteArray data;
    QStringList values;
    int rpm;

    if (!_ecuCommand("d\r\n", NULL, &data)) {
        _ecuDisconnected();
        return;
    }
    values = QString(data.trimmed()).split(' ');
    if (values.size() < 4) {
        _ecuDisconnected();
        return;
    }

    rpm = values[0].toInt();
    _ui->lRPM->setText(QString("%1 RPM").arg(rpm));
    _ui->lIgnitionAdvance->setText(QString::fromUtf8("%1 °").arg(values[1]));
    _ui->lCrankAccel->setText(values[2]);

    /* Fancy podświetlanie aktywnego fragmentu mapy zapłonu */
    for(int col = 0; col < _ui->twIgnitionMap->columnCount(); col++) {
        QColor color;

        for(int row = 0; row < _ui->twIgnitionMap->rowCount(); row++) {
            if (((rpm / 500) == col) && (row == _ui->cbCurrentMap->currentIndex())) {
                color = QColor(Qt::green);
            }
            else {
                color = QColor(Qt::white);
            }


            if (!_ui->twIgnitionMap->item(row, col)) {
                _ui->twIgnitionMap->setItem(row, col, new QTableWidgetItem());
            }
            _ui->twIgnitionMap->item(row, col)->setBackgroundColor(color);
        }
    }
}

void WndMain::_readEcuMap() {
    QByteArray data;
    QStringList rows;
    QStringList items;

    if (!_ecuCommand("r\r\n", NULL, &data)) {
        return;
    }

    rows = QString(data).trimmed().split(';');
    if (_ui->twIgnitionMap->rowCount() != (rows.count() - 1)) {
        qDebug() << "Wrong rowCount" << _ui->twIgnitionMap->rowCount() << "!=" << (rows.count() - 1);
        return;
    }
    for(int i = 0; i < rows.count() - 1; i++) {
        items = rows.at(i).trimmed().split(' ');

        for(int j = 0; j < items.count(); j++) {
            if (!_ui->twIgnitionMap->item(i, j)) {
                _ui->twIgnitionMap->setItem(i, j, new QTableWidgetItem(items.at(j)));
            }
            else {
                _ui->twIgnitionMap->item(i, j)->setText(items.at(j));
            }
        }
    }
}

void WndMain::_writeEcuMap() {
    uint8_t exitCode;
    QString command = "w";

    for(int i = 0; i < _ui->twIgnitionMap->rowCount(); i++) {
        for(int j = 0; j < _ui->twIgnitionMap->columnCount(); j++) {
            command.append(QString("%1").arg(_ui->twIgnitionMap->item(i, j)->text().toInt(), 2, 16, QLatin1Char('0')));
        }

        command.append(";");
    }

    command.append("\r\n");

    //qDebug() << command;

    if (!_ecuCommand(command.toAscii(), &exitCode, NULL)) {
        QMessageBox::critical(this, "Zapis mapy do ECU", QString::fromUtf8("Błąd zapisu danych do ECU (timeout podczas wykonywania polecenia)"));
        return;
    }

    if (exitCode != 0) {
        QMessageBox::critical(this, "Zapis mapy do ECU", QString::fromUtf8("Błąd zapisu danych do ECU (kod błędu = %1)").arg(exitCode));
    }
}

void WndMain::_readParams() {
    uint16_t data;

    data = _readEcuParam(PARAM_IGN_CUT_OFF_START);
    if (data != 0xFFFF)
        _ui->sbCuttOffStart->setValue(data);

    data = _readEcuParam(PARAM_IGN_CUT_OFF_END);
    if (data != 0xFFFF)
        _ui->sbCuttOffEnd->setValue(data);

    data = _readEcuParam(PARAM_DYNAMIC_ON);
    if (data != 0xFFFF)
        _ui->sbDynamicOn->setValue(data);

    data = _readEcuParam(PARAM_DYNAMIC_OFF);
    if (data != 0xFFFF)
        _ui->sbDynamicOff->setValue(data);

    data = _readEcuParam(PARAM_CURRENT_MAP);
    if (data < _ui->cbCurrentMap->count())
        _ui->cbCurrentMap->setCurrentIndex(data);

    _readImmoKeys();
}

void WndMain::_writeParams() {
    _writeEcuParam(PARAM_IGN_CUT_OFF_START, _ui->sbCuttOffStart->value());
    _writeEcuParam(PARAM_IGN_CUT_OFF_END, _ui->sbCuttOffEnd->value());
    _writeEcuParam(PARAM_DYNAMIC_ON, _ui->sbDynamicOn->value());
    _writeEcuParam(PARAM_DYNAMIC_OFF, _ui->sbDynamicOff->value());
    _writeEcuParam(PARAM_CURRENT_MAP, _ui->cbCurrentMap->currentIndex());

    _writeImmoKeys();
}

void WndMain::_readImmoKeys() {
    QByteArray data;
    QStringList keys;

    if (!_ecuCommand("k\r\n", NULL, &data)) {
        return;
    }

    keys = QString(data.trimmed()).split(' ');
    if (keys.count() > 0) {
        _ui->leImmoKey0->setText(keys[0]);
    }

    if (keys.count() > 1) {
        _ui->leImmoKey1->setText(keys[1]);
    }
}

void WndMain::_writeImmoKeys() {
    uint8_t err;
    QByteArray command;

    command = QString("i%1 %2\r\n").arg(_ui->leImmoKey0->text(), 12, '0').arg(_ui->leImmoKey1->text(), 12, '0').toAscii();

    if (!_ecuCommand(command, &err, NULL)) {
        QMessageBox::critical(this, QString::fromUtf8("Zapis kodów immobilizera do ECU"), QString::fromUtf8("Błąd zapisu danych do ECU (timeout)"));
        return;
    }

    if (err != 0) {
        QMessageBox::critical(this, QString::fromUtf8("Zapis kodów immobilizera do ECU"), QString::fromUtf8("Błąd zapisu danych do ECU (kod błędu = %1)").arg(err));
    }
}

bool WndMain::_ecuCommand(QByteArray command, uint8_t *exitCode, QByteArray *result) {
    QByteArray data;
    bool done;

    _serial->write(command);

    if(!_serial->waitForReadyRead(1000)) { /* ECU ma sekundę na odpowiedź */
        return false;
    }

    /* Powinniśmy dostac:
     * \r\n
     * Dane, linia 1\r\n
     * Dane, linia 2\r\n
     * ....
     * Dane, linia n\r\n
     * KodWyjsca>
    */

    done = false;

    while (!done) {
        data = _serial->readAll();
        //qDebug() << data;
        if (data.isEmpty()) {
            if (!_serial->waitForReadyRead(1000))
                return false;
            continue;
        }

        /* Szukamy końca danych */
        if (data.endsWith('>')) {
            /* Koniec danych, odczytujemy kod błędu, jeżeli mamy go gdzie zapisać */
            if (exitCode != NULL) {
                *exitCode = data.right(3).left(2).toInt(0, 16);
            }

            /* Dopisujemy reszte danych */
            if (result != NULL) {
                result->append(data.left(data.length() - 3));
            }
            done = true;
        }
        else {
            /* Dopisujemy dane do wyniku */
            if (result != NULL) {
                result->append(data);
            }
        }
    }
    return true;
}

uint16_t WndMain::_readEcuParam(int id) {
    uint8_t err;
    QByteArray data;

    if (!_ecuCommand(QString("g%1\r\n").arg(id, 2, 16, QLatin1Char('0')).toAscii(), &err, &data)) {
        QMessageBox::critical(this, "Odczyt parametru", QString::fromUtf8("Błąd odczytu parametru %1 z ECU (timeout)").arg(id));
        return 0xFFFF;
    }

    if (err != 0) {
        QMessageBox::critical(this, "Odczyt parametru", QString::fromUtf8("Błąd odczytu parametru %1 z ECU (kod błędu = %2)").arg(id).arg(err));
        return 0xFFFF;
    }

    return data.trimmed().toUInt(0, 16);
}

void WndMain::_writeEcuParam(int id, uint16_t value) {
    uint8_t err;

    if (!_ecuCommand(QString("s%1%2\r\n").arg(id, 2, 16, QLatin1Char('0')).arg(value, 4, 16, QLatin1Char('0')).toAscii(), &err, NULL)) {
        QMessageBox::critical(this, "Zapis parametru", QString::fromUtf8("Błąd zapisu parametru %1 do ECU (kod błędu = %2)").arg(id).arg(err));
        return;
    }
}
