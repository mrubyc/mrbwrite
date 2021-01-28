/*! @file
  @brief
  mruby/c irep file writer.

  <pre>
  Copyright (C) 2017-2021 Kyushu Institute of Technology.
  Copyright (C) 2017-2021 Shimane IT Open-Innovation Center.

  This file is distributed under BSD 3-Clause License.

  </pre>
*/

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

#define VERBOSE(s) if( opt_verbose_ ) { qout_ << s << endl; }


//================================================================
/*! constructor

  @param	argc	command line size
  @param	argv	pointer to command line options
*/
MrbWrite::MrbWrite( int argc, char *argv[] )
  : QCoreApplication( argc, argv ),
    qout_(stdout),
    serial_baud_rate_(57600)
{
  setApplicationName("mrbwrite");
  setApplicationVersion("1.01");

  /*
    parse command line options.
  */
  QCommandLineParser parser;
  parser.setApplicationDescription("mruby/c program writer.");
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

  parser.process(*this);

  mrb_files_ = parser.positionalArguments();
  line_ = parser.value( lineOption );
  if( parser.isSet( baudRateOption ) ) {
    serial_baud_rate_ = parser.value( baudRateOption ).toInt();
  }
  opt_verbose_ = parser.isSet(verboseOption);
  opt_show_lines_ = parser.isSet(showLinesOption);

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
    qout_ << tr("must specify line (-l option)") << endl;
    goto DONE;
  }

  /*
    check .mrb files exist?
  */
  if( mrb_files_.isEmpty() ) {
    qout_ << tr("must specify .mrb file.") << endl;
    goto DONE;
  }

  flag_error = 0;
  foreach( const QString filename, mrb_files_ ) {
    if( ! QFile::exists( filename )) {
      qout_ << tr("File not found '") << filename << "'." << endl;
      flag_error = 1;
    }
  }
  if( flag_error ) goto DONE;

  /*
    connect target
  */
  if( connect_target() != 0 ) goto DONE;
  clear_bytecode();

  /*
    open .mrb files and write target.
  */
  foreach( const QString filename, mrb_files_ ) {
    QFile file( filename );
    if( !file.open( QIODevice::ReadOnly ) ) {
      qout_ << tr("Can't open file '%1'.").arg(filename) << endl;
      goto DONE;
    }
    qout_ << tr("Writeing %1").arg(filename) << endl;
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

  qout_ << tr("Start connection.") << endl;

 REDO:
  if( ++n_try > 10 ) {
    qout_ << tr("Try over 10 times.") << endl;
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
    qout_ << tr("Can't open serial port line.") << endl;
    return 1;
  case 2:
    qout_ << tr("Can't set baud rate.") << endl;
    return 1;
  }
  VERBOSE("Serial port is ready.");

  // trying to connect target
  VERBOSE("Trying to connect target.");
  for( i = 0; i < 10; i++ ) {
    if( serial_port_.error() != QSerialPort::NoError ) {
      VERBOSE("Serial port error has detected. Retrying.");
      serial_port_.close();
      sleep_ms( 200 );
      goto REDO;
    }
    serial_port_.clear();
    serial_port_.write("\r\n");
    VERBOSE("\nSend '\\r\\n' to target for connection start.");
    qout_ << ".";
    qout_.flush();

    QString r = get_line();
    VERBOSE(tr("Receive '%1'").arg(r.trimmed()));
    if( r.startsWith("+OK mruby/c")) break;
  }
  qout_ << "\r                 \r";
  if( i == 10 ) {
    qout_ << tr("Can't connect target device.") << endl;
    return 1;
  }
  qout_ << tr("OK.") << endl;

  // check target version
  serial_port_.write("version\r\n");
  QString ver = get_line();
  if( !ver.startsWith("+OK mruby/c PSoC_5LP v1.00 ") &&
      !ver.startsWith("+OK mruby/c v2.1")) {
    qout_ << tr("version mismatch.") << endl;
    qout_ << tr(" require 1.00") << endl;
    qout_ << tr(" connected ") << ver << endl;

    return 1;
  }
  VERBOSE("Target firmware version OK.");

  return 0;
}


//================================================================
/*! clear existed mruby/c bytecode.
*/
int MrbWrite::clear_bytecode()
{
  qout_ << tr("Clear existed bytecode.") << endl;

  serial_port_.write("clear\r\n");
  VERBOSE(tr("Send 'clear'"));

  QString r = get_line();
  VERBOSE(tr("Receive '%1'").arg(r.trimmed()));
  if( !r.startsWith("+OK")) {
    qout_ << tr("Bytecode clear error.") << endl;
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

  while( 1 ) {
    QString r = get_line();
    if( r.startsWith("+DONE")) break;
    qout_ << r;
  }

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

  QString s = QString("write %1\r\n").arg( filesize );
  serial_port_.write(s.toLocal8Bit());
  VERBOSE(tr("Send '%1'").arg(s.trimmed()));
  QString r = get_line();
  VERBOSE(tr("Receive '%1'").arg(r.trimmed()));
  if( !r.startsWith("+OK ")) {
    qout_ << tr("command error. '%1'").arg(r.trimmed()) << endl;
    return 1;
  }

  for( int i = 0; i < filesize; i++ ) {
    char ch;
    file.getChar(&ch);
    serial_port_.putChar(ch);
    serial_port_.waitForBytesWritten();
  }
  VERBOSE(tr("Send %1 bytes done.").arg(filesize));
  r = get_line();
  VERBOSE(tr("Receive '%1'").arg(r.trimmed()));
  if( !r.startsWith("+DONE")) {
    qout_ << tr("transfer error. '%1'").arg(r.trimmed()) << endl;
    return 1;
  }
  qout_ << tr("OK.") << endl;

  return 0;
}



//================================================================
/*! execute program

*/
void MrbWrite::execute_program()
{
  qout_ << tr("Start mruby/c program.") << endl;

  serial_port_.write("execute\r\n");
  QString r = get_line();
  if( !r.startsWith("+OK ")) {
    qout_ << tr("execute error. '%1'").arg(r.trimmed()) << endl;
  }
  qout_ << tr("OK.") << endl;
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

  @return QString
*/
QString MrbWrite::get_line()
{
  for( int i = 0; i < 100; i++ ) {
    if( serial_port_.canReadLine()) {
      return serial_port_.readLine();
    }
    sleep_ms(10);
  }

  return QString("");
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
    qout_ << s << endl;
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
