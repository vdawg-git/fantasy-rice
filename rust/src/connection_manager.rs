use anyhow::Context;
use std::fs::remove_file;
use std::io::Write;
use std::os::unix::net::{UnixListener, UnixStream};

// Wrapper to handle multiple client connections
#[derive(Debug)]
pub struct ConnectionManager {
    listener: UnixListener,
    clients: Vec<UnixStream>,
}

impl ConnectionManager {
    pub fn new(socket_path: &str) -> Result<Self, Box<dyn std::error::Error>> {
        let _ = remove_file(socket_path);
        let listener = UnixListener::bind(socket_path)
            .context(format!("Failed to create socket: {}", socket_path))?;

        // Make the listener non-blocking so we can check for new connections
        listener.set_nonblocking(true)?;

        Ok(ConnectionManager {
            listener,
            clients: Vec::new(),
        })
    }

    fn accept_new_connections(&mut self) {
        // Try to accept new connections (non-blocking)
        loop {
            match self.listener.accept() {
                Ok((stream, _)) => {
                    println!(
                        "New client connected (total clients: {})",
                        self.clients.len() + 1
                    );
                    self.clients.push(stream);
                }
                Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                    // No more pending connections
                    break;
                }
                Err(e) => {
                    eprintln!("Error accepting connection: {}", e);
                    break;
                }
            }
        }
    }

    pub fn broadcast_message(&mut self, message: &str) {
        // First, try to accept any new connections
        self.accept_new_connections();

        if self.clients.is_empty() {
            return; // No clients to send to
        }

        // Send message to all clients, removing any that have disconnected
        let mut index = 0;
        while index < self.clients.len() {
            match self.clients[index].write_all(message.as_bytes()) {
                Ok(_) => {
                    index += 1; // Successfully sent, move to next client
                }
                Err(error) => {
                    println!(
                        "Client {} disconnected: {} (remaining clients: {})",
                        index,
                        error,
                        self.clients.len() - 1
                    );
                    self.clients.remove(index);
                    // Don't increment i since we removed an element
                }
            }
        }
    }

    pub fn client_count(&self) -> usize {
        self.clients.len()
    }
}
