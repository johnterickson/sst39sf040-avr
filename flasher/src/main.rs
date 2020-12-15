use std::{error::Error, io::{self, BufRead, BufReader, Write}, thread::sleep, time::Duration};

use serialport::prelude::*;

fn main() -> Result<(), Box<dyn Error>>{
    let mut settings = SerialPortSettings::default();
    settings.baud_rate = 115_200;
    settings.timeout = Duration::from_secs(10);
    settings.data_bits = DataBits::Eight;
    settings.parity = Parity::None;
    settings.stop_bits = StopBits::One;
    settings.flow_control = FlowControl::None;


    println!("Ports:");
    for port in serialport::available_ports()? {
        println!(" {}", port.port_name);
    }

    let port_name = std::env::args().nth(1).unwrap();
    print!("Port: {}", port_name);   
    let mut port = serialport::open_with_settings(&port_name, &settings)?;
    println!(" opened!");

    print!("Parsing file..."); std::io::stdout().flush()?;

    let mut bytes = Vec::new();

    let stdin = io::stdin();
    let mut stdin = stdin.lock();
    let mut line = String::new();
    stdin.read_line(&mut line)?;
    match line.as_str().trim() {
        "v2.0 raw" => Ok(()),
        other => Err(format!("Unexpected header: {}", other)),
    }?;

    line.clear();
    while let Ok(bytes_read) = stdin.read_line(&mut line) {
        if bytes_read == 0 {
            break;
        }
        //dbg!(&line);
        match line.as_str().trim() {
            line if line.chars().next() == Some('#') => {
                // ignore comment
            },
            line if line.chars().any(|c| c == '*') => {
                let mut tokens = line.split('*');
                let count = usize::from_str_radix(tokens.next().expect("Expected repeat count").trim(), 10)?;
                let byte = u8::from_str_radix(tokens.next().expect("Expected repeated byte value").trim(), 16)?;
                for _ in 0..count {
                    bytes.push(byte);
                }
            }
            line => {
                for byte in line.split_whitespace().map(|b| u8::from_str_radix(b, 16)) {
                    bytes.push(byte?);
                    
                }
            }
        }
        line.clear();
    }

    println!("Parsed {} bytes.", bytes.len()); std::io::stdout().flush()?;

    // reboot
    port.write_request_to_send(false)?;
    port.write_data_terminal_ready(false)?;
    sleep(Duration::from_millis(100));
    port.write_request_to_send(true)?;
    port.write_data_terminal_ready(true)?;

    let mut serial_reader = port.try_clone()?;
    
    let mut read_line = || -> Result<_,std::io::Error> {
        let mut serial_line = String::new();
        let mut byte = [0u8];
        loop {
            serial_reader.read(&mut byte)?;
            //print!("{}", byte[0] as char);
            if byte[0] == b'\n' {
                break;
            }
            serial_line.push(byte[0] as char);
        }
        Ok(serial_line)
    };

    println!("{}", read_line()?);
    
    print!("Fetching info: "); std::io::stdout().flush()?;
    write!(port, "i\n")?; port.flush()?;
    println!("{}", read_line()?);

    print!("Erasing: ");
    write!(port, "e\n")?; port.flush()?;
    println!("{}", read_line()?);

    print!("Setting Address: ");
    write!(port, "a0\n")?; port.flush()?;
    println!("{}", read_line()?);

    for b in &bytes {
        write!(port, "w{:02x}\n", b)?; port.flush()?;
        let response = read_line()?;
        assert_eq!(*b, u8::from_str_radix(response.trim(), 16).unwrap());
    }

    
    Ok(())
}
