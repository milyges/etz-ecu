#ifndef WNDMAIN_H
#define WNDMAIN_H

#include <QMainWindow>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QTimer>
#include <stdint.h>

#define PARAM_IGN_CUT_OFF_START  0
#define PARAM_IGN_CUT_OFF_END    1
#define PARAM_DYNAMIC_ON         2
#define PARAM_DYNAMIC_OFF        3
#define PARAM_CURRENT_MAP        4
#define PARAM_IMMO_ENABLED       5
#define PARAM_CRANK_OFFSET       6
#define PARAM_COUNT              7

namespace Ui {
    class WndMain;
}

class WndMain : public QMainWindow {
    Q_OBJECT

public:
    explicit WndMain(QWidget *parent = 0);
    ~WndMain();

protected:
    void changeEvent(QEvent *e);

private slots:
    bool _ecuConnect(const QSerialPortInfo & port);
    void _ecuDisconnected(void);
    void _scanPorts(void);
    void _updateLiveData(void);

    void _readEcuMap(void);
    void _writeEcuMap(void);

    void _readParams(void);
    void _writeParams(void);

    void _readImmoKeys(void);
    void _writeImmoKeys(void);

private:
    Ui::WndMain * _ui;
    QTimer * _connectTimer;
    QTimer * _liveDataTimer;

    QSerialPort * _serial;

    bool _ecuCommand(QByteArray command, uint8_t * exitCode, QByteArray * result);
    uint16_t _readEcuParam(int id);
    void _writeEcuParam(int id, uint16_t value);

};

#endif // WNDMAIN_H
