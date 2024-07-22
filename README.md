# Proxy Server

## Overview

This proxy server is a simple HTTP proxy that handles client requests, checks if the requested URL is allowed, caches responses using an LRU (Least Recently Used) cache mechanism, and serves cached responses when available. The server listens for incoming HTTP requests, forwards requests to the remote server if not cached, and returns the response back to the client. It also includes a feature to print cached responses on demand.

## Features

- **HTTP Proxying**: Forwards HTTP GET requests to remote servers.
- **Caching**: Implements an LRU cache to store responses and avoid redundant network requests.
- **Domain Filtering**: Only allows requests to predefined allowed domains.
- **Signal Handling**: Gracefully shuts down the server on SIGINT (Ctrl+C) and frees resources.
- **Cache Management**: Print the contents of the cache on demand.

## Getting Started

### Prerequisites

- A UNIX-like operating system (Linux, macOS, etc.)
- GCC compiler or any standard C compiler
- Basic understanding of networking and HTTP

### Installation

1. **Clone the repository** (replace `<repository_url>` with the actual URL):
    ```sh
    git clone <repository_url>
    ```

2. **Navigate to the project directory**:
    ```sh
    cd <project_directory>
    ```

3. **Compile the program**:
    ```sh
    gcc -o proxy_server proxy.c -lpthread
    ```

### Configuration

The list of allowed domains is defined in the `allowed_domains` array in the source code. Modify this list as needed to control which domains are accessible through the proxy server.Athough this code is only capable of serving the HTTP request only. **NOTE** : First turn Your manual proxy configuration settings on and change the port to operate on port 8080. 

### Running the Server

1. **Start the server**:
    ```sh
    ./proxy_server
    ```

2. The server will start listening on port 8080. You can change the port by modifying the `PORT` macro in the source code.

### Usage

- **Sending Requests**: You can use any HTTP client or web browser to send requests to `http://localhost:8080`.
- **Cache Management**: Type `cache` into the terminal where the server is running to print the current cache contents.

### Stopping the Server

To stop the server gracefully, press `Ctrl+C`. The server will handle the SIGINT signal, close open sockets, and shut down cleanly.

## Code Structure

- **`proxy.c`**: Contains the main program logic including request handling, caching, and signal handling.
- **`CacheEntry` Structure**: Manages cached entries with LRU mechanism.
- **`print_cache` Thread**: Prints cache contents based on user input.

## Troubleshooting

- **Port Already in Use**: If you encounter an error related to the port being in use, ensure no other process is using port 8080 or modify the `PORT` macro to use a different port.
- **Permission Denied**: Ensure you have the necessary permissions to bind to the specified port.

## Contributing

Feel free to open issues or submit pull requests if you have improvements or bug fixes. Contributions are welcome!

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Acknowledgements

This project uses standard libraries available in UNIX-like operating systems and is inspired by common proxy server implementations.

## Contact

For questions or suggestions, you can reach out to [Your Name] at [your.email@example.com].

