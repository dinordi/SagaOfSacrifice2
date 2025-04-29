# Use a base image with GCC and CMake
FROM gcc:latest AS builder

# Install dependencies (CMake and Boost libraries)
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    libboost-system-dev \
    libboost-asio-dev \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory
WORKDIR /app

# Copy the server source code
COPY . .

# Create a build directory and build the project
RUN mkdir build && cd build && \
    cmake .. && \
    make

# --- Runtime Stage ---
# Use a smaller base image for the final container
FROM debian:bullseye-slim

# Install runtime dependencies (Boost libraries)
RUN apt-get update && apt-get install -y --no-install-recommends \
    libboost-system1.74.0 \
    libboost-asio1.74.0 \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory
WORKDIR /app

# Copy the built executable from the builder stage
COPY --from=builder /app/build/SagaServer .

# Expose the port the server listens on (assuming 12345 from main.cpp)
EXPOSE 8282

# Command to run the server
CMD ["./SagaServer"]