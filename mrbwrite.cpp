/*! @file
  @brief
  mruby/c irep file writer.

  <pre>
  Copyright (C) 2017- Kyushu Institute of Technology.
  Copyright (C) 2017- Shimane IT Open-Innovation Center.

  This file is distributed under BSD 3-Clause License.

  </pre>
*/

#define APPLICATION_VERSION "1.2.0"
#define PROTOCOL_VERSION "MRBW1.2"

#include <stdio.h>
#include <stdlib.h>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTextStream>
#include <QTimer>
#include <QSerialPortInfo>
#include <QSerialPort>
#include <QFile>
#include <QDebug>

#include "mrbwrite.h"

#define VERBOSE(s) if( opt_verbose_ ) { qout_ << s << Qt::endl; }

const char * const STR_CANCEL = "\x018";


//================================================================
/*! constructor

  @param	argc	command line size
  @param	argv	pointer to command line options
*/
MrbWrite::MrbWrite( int argc, char *argv[] )
  : QCoreApplication( argc, argv ),
    qout_(stdout),
    opt_timeout_(5),
    serial_baud_rate_(57600)
{
  setApplicationName("mrbwrite");
  setApplicationVersion(APPLICATION_VERSION);

  /*
    parse command line options.
  */
  QCommandLineParser parser;
  parser.setApplicationDescription("mruby/c program writer. version " APPLICATION_VERSION);
  parser.addHelpOption();
  parser.addVersionOption();

  parser.addPositionalArgument("mrbfile ...", tr("mrb file to write."));

  QCommandLineOption lineOption(QStringList() << "l" << "line",
                                tr("Device name. (e.g. COM1)"), tr("line"));
  parser.addOption(lineOption);

  QCommandLineOption baudRateOption(QStringList() << "s" << "speed",
                                tr("Baud rate.(e.g. 57600)"), tr("speed"));
  parser.addOption(baudRateOption);

  QCommandLineOption verboseOption("verbose", tr("Verbose mode."));
  parser.addOption(verboseOption);

  QCommandLineOption showLinesOption("showline", tr("Show all lines."));
  parser.addOption(showLinesOption);

  QCommandLineOption timeoutOption("timeout",
				   tr("Command timeout."), tr("timeout"));
  parser.addOption(timeoutOption);

  parser.process(*this);

  mrb_files_ = parser.positionalArguments();
  line_ = parser.value( lineOption );
  if( parser.isSet( baudRateOption ) ) {
    serial_baud_rate_ = parser.value( baudRateOption ).toInt();
  }
  opt_verbose_ = parser.isSet(verboseOption);
  opt_show_lines_ = parser.isSet(showLinesOption);
  if( parser.isSet( timeoutOption ) ) {
    opt_timeout_ = parser.value( timeoutOption ).toInt();
  }

  /*
    start user program main function run()
  */
  QTimer::singleShot(0, this, SLOT(run()));
}


//================================================================
/*! user main function

*/
void MrbWrite::run()
{
  int flag_error = 1;

  /*
    process --showline option.
  */
  if( opt_show_lines_ ) {
    show_lines();
    goto DONE;
  }

  /*
    check --line option is specified.
  */
  if( line_.isEmpty() ) {
    qout_ << tr("must specify line (-l option)") << Qt::endl;
    goto DONE;
  }

  /*
    check .mrb files exist?
  */
  if( mrb_files_.isEmpty() ) {
    qout_ << tr("must specify .mrb file.") << Qt::endl;
    goto DONE;
  }

  flag_error = 0;
  foreach( const QString filename, mrb_files_ ) {
    if( ! QFile::exists( filename )) {
      qout_ << tr("File not found '") << filename << "'." << Qt::endl;
      flag_error = 1;
    }
  }
  if( flag_error ) goto DONE;

  /*
    connect target
  */
  if( connect_target() != 0 ) goto DONE;

  /*
    clear existed bytecode.
  */
  flag_error = clear_bytecode();
  if( flag_error && !target_rite_version_.isEmpty() ) goto DONE;

  /*
    open .mrb files and write target.
  */
  foreach( const QString filename, mrb_files_ ) {
    QFile file( filename );
    if( !file.open( QIODevice::ReadOnly ) ) {
      qout_ << tr("Can't open file '%1'.").arg(filename) << Qt::endl;
      goto DONE;
    }

    qout_ << tr("Writing %1").arg(filename) << Qt::endl;
    flag_error = write_file( file );
    file.close();

    if( flag_error ) goto DONE;
  }

  /*
    display program list
   */
  show_prog();

  /*
    execute program
  */
  execute_program();

  /*
    finalizer
  */
 DONE:
  if( serial_port_.isOpen() ) {
    VERBOSE( tr("Closing serial port."));
    serial_port_.close();
  }
  VERBOSE( tr("Program end"));
  exit( flag_error );
}


//================================================================
/*! connect target board.

  @retval    int
*/
int MrbWrite::connect_target()
{
  int n_try = 0;
  int i, ret;

  qout_ << tr("Start connection.") << Qt::endl;

 REDO:
  if( ++n_try > 10 ) {
    qout_ << tr("Try over 10 times.") << Qt::endl;
    return 1;
  }

  // trying to open serial port.
  VERBOSE( tr("Trying to open '%1'.").arg(line_) );
  for( i = 0; i < 50; i++ ) {
    ret = setup_serial_port();
    if( ret != 1 ) break;
    sleep_ms( 100 );
  }
  switch( ret ) {
  case 1:
    qout_ << tr("Can't open serial port line.") << Qt::endl;
    return 1;
  case 2:
    qout_ << tr("Can't set baud rate.") << Qt::endl;
    return 1;
  }
  VERBOSE("Serial port is ready.");

  // trying to connect target
  VERBOSE("Trying to connect target.");
  const int MAX_CONN = 10;
  for( i = 0; i < MAX_CONN; i++ ) {
    if( serial_port_.error() != QSerialPort::NoError ) {
      VERBOSE("Serial port error has detected. Retrying.");
      serial_port_.close();
      sleep_ms( 100 );
      goto REDO;
    }
    sleep_ms( 100 );
    serial_port_.clear();
    serial_port_.write("\r\n");
    serial_port_.flush();
    VERBOSE("\n==> '\\r\\n' to target for connection start.");
    qout_ << ".";
    qout_.flush();

    QString r = get_line(50);
    VERBOSE(tr("<== '%1'").arg(r.trimmed()));
    if( r.startsWith("+OK mruby/c") ) break;
  }
  qout_ << "\r                 \r";
  if( i == MAX_CONN ) {
    qout_ << tr("Can't connect target device.") << Qt::endl;
    return 1;
  }
  qout_ << tr("OK.") << Qt::endl;
  sleep_ms( 100 );
  serial_port_.clear();

  // check target version
  VERBOSE(tr("Check target version."));
  serial_port_.write("version\r\n");
  VERBOSE(tr("==> 'version'"));

  QString target_version = get_line().trimmed();
  VERBOSE(tr("<== '%1'").arg(target_version));

  // (for backword compatibility)
  if( target_version.startsWith("+OK mruby/c PSoC_5LP v1.00 ") ||
      target_version.startsWith("+OK mruby/c v2.1")) {
    ret = 0;
  } else {
    QStringList vers = target_version.split(' ');
    target_rite_version_ = vers[3];
    ret = (vers[4] != PROTOCOL_VERSION);
  }

  if( ret ) {
    qout_ << tr("protocol version mismatch.") << Qt::endl;
  } else {
    VERBOSE(tr("Target firmware version OK."));
  }

  return ret;
}


//================================================================
/*! clear existed mruby/c bytecode.
*/
int MrbWrite::clear_bytecode()
{
  qout_ << tr("Clear existed bytecode.") << Qt::endl;

  if( chat("clear") < 0 ) {
    qout_ << tr("Bytecode clear error.") << Qt::endl;
    return 1;
  }
  VERBOSE("Clear bytecode OK.");
  return 0;
}


//================================================================
/*! show program list
*/
int MrbWrite::show_prog()
{
  serial_port_.write("showprog\r\n");
  VERBOSE(tr("==> 'showprog'"));

  QString r;

  while( 1 ) {
    r = get_line();
    if( r.startsWith("+DONE")) break;
    if( r.startsWith( STR_CANCEL )) break;
    qout_ << r;
  }
  VERBOSE(tr("<== '%1'").arg(r.trimmed()));

  return 0;
}


//================================================================
/*! write a file.

  @param	file	file I/O object.
  @retval	int	0: no error
*/
int MrbWrite::write_file( QIODevice &file )
{
  int filesize = file.size();
  QByteArray header = file.read(8);

  // check RITE version.
  if( !target_rite_version_.isEmpty() ) {
    if( target_rite_version_ != header ) {
      qout_ << "mrb file RITE version mismatch." << Qt::endl;
      return 2;
    }
    VERBOSE(tr("RITE version '%1' check OK.").arg(target_rite_version_));
  }

  // send "write" command
  QString s = QString("write %1").arg( filesize );
  if( chat(s.toLocal8Bit()) < 0 ) {
    qout_ << "command error." << Qt::endl;
    return 1;
  }

  // send mrb file.
  serial_port_.write( header );

  for( int i = 0; i < filesize-8; i++ ) {
    char ch;
    file.getChar(&ch);
    serial_port_.putChar(ch);
    serial_port_.waitForBytesWritten();
  }
  VERBOSE(tr("Send %1 bytes done.").arg(filesize));

  // check status.
  while(1) {
    QString r = get_line();
    VERBOSE(tr("<== '%1'").arg(r.trimmed()));

    if( r.startsWith( STR_CANCEL )) {
      qout_ << tr("transfer timeout") << Qt::endl;
      return 1;
    }
    if( r.startsWith("+DONE")) break;
    if( r.startsWith("-ERR")) {
      qout_ << tr("transfer error. '%1'").arg(r.trimmed()) << Qt::endl;
      return 1;
    }
    qout_ << r << Qt::endl;
  }

  qout_ << tr("OK.") << Qt::endl;
  return 0;
}


//================================================================
/*! execute program

*/
void MrbWrite::execute_program()
{
  qout_ << tr("Start mruby/c program.") << Qt::endl;

  if( chat("execute") >= 0 ) {
    qout_ << tr("OK.") << Qt::endl;
  } else {
    qout_ << tr("execute error.") << Qt::endl;
  }
}


//================================================================
/*! open a communication port.

  @retval	int	0: no error
*/
int MrbWrite::setup_serial_port()
{
  serial_port_.setPortName( line_ );

  if( !serial_port_.open( QIODevice::ReadWrite )) return 1;
  if( !serial_port_.setBaudRate( serial_baud_rate_ )) return 2;

  serial_port_.setDataBits( QSerialPort::Data8 );
  serial_port_.setParity( QSerialPort::NoParity );
  serial_port_.setStopBits( QSerialPort::OneStop );
  serial_port_.setFlowControl( QSerialPort::HardwareControl );

  return 0;
}


//================================================================
/*! get a line from serial port with timeout.

  @param	timeout_count	timeout counter.
  @return QString
*/
QString MrbWrite::get_line( int timeout_count )
{
  if( timeout_count == 0 ) {
    timeout_count = opt_timeout_ * 100;
  }

  for( int i = 0; i < timeout_count; i++ ) {
    if( serial_port_.canReadLine()) {
      return serial_port_.readLine();
    }
    sleep_ms(10);
  }

  return QString(STR_CANCEL);	// Timeout
}


//================================================================
/*! chat

  @param cmd    send command.
  @return int   0=+OK, 1=+DONE, -1=-ERR, -2=Timeout
*/
int MrbWrite::chat( const char *cmd )
{
  VERBOSE(tr("==> '%1'").arg(cmd));

  serial_port_.write(cmd);
  serial_port_.write("\r\n");

  while( 1 ) {
    QString r = get_line();
    VERBOSE(tr("<== '%1'").arg(r.trimmed()));
    if( r.startsWith("+OK")) return 0;
    if( r.startsWith("+DONE")) return 1;
    if( r.startsWith("-ERR")) return -1;
    if( r.startsWith( STR_CANCEL )) {
      qout_ << "TIMEOUT!" << Qt::endl;
      return -2;
    }
    qout_ << r;
  }
}


//================================================================
/*! show device list.

*/
void MrbWrite::show_lines()
{
  foreach( const QSerialPortInfo &info, QSerialPortInfo::availablePorts() ) {
    QString s = QString("%1\t%2\t%3")
      .arg(info.portName())
      .arg(info.description())
      .arg(info.manufacturer());
    qout_ << s << Qt::endl;
  }
}


//================================================================
/*! sleep (ms)

*/
void MrbWrite::sleep_ms( int ms )
{
  QEventLoop loop;
  QTimer::singleShot( ms, &loop, SLOT(quit()) );
  loop.exec();
}
