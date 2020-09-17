/*! @file
  @brief
  mruby/c irep file writer.

  <pre>
  Copyright (C) 2017-2020 Kyushu Institute of Technology.
  Copyright (C) 2017-2020 Shimane IT Open-Innovation Center.

  This file is distributed under BSD 3-Clause License.

  </pre>
*/
#include <QCoreApplication>
#include <QStringList>
#include <QTextStream>
#include <QSerialPort>
#include <QIODevice>


//================================================================
/*! MrbWrite class.
*/
class MrbWrite : public QCoreApplication
{
  Q_OBJECT

public:
  MrbWrite( int argc, char *argv[] );
  void sleep_ms( int ms );

public slots:
  void run();

private:
  QTextStream qout_;		//!< console output stream.
  bool opt_verbose_;		//!< command line option --verbose
  bool opt_show_lines_;		//!< command line option --showline
  QString line_;		//!< command line option parameter -l
  QStringList mrb_files_;	//!< .mrb file filename list.
  QSerialPort serial_port_;	//!< serial port object.
  int serial_baud_rate_;	//!< serial baud rate.

  int connect_target();
  int clear_bytecode();
  int show_prog();
  int write_file( QIODevice &file );
  void execute_program();
  int setup_serial_port();
  QString get_line();
  void show_lines();
};
