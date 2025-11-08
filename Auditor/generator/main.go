// Testserver only for testing json generator ./main.py 
package main

import (
	"fmt"
	"math/rand"
	"net/http"
	"time"
)

// A list of common HTTP status codes (2xx, 3xx, 4xx, 5xx) to choose from.
var statusCodes = []int{
	http.StatusOK,                      // 200: OK
	http.StatusCreated,                 // 201: Created
	http.StatusAccepted,                // 202: Accepted
	http.StatusNoContent,               // 204: No Content
	http.StatusMovedPermanently,        // 301: Moved Permanently
	http.StatusFound,                   // 302: Found
	http.StatusBadRequest,              // 400: Bad Request
	http.StatusUnauthorized,            // 401: Unauthorized
	http.StatusForbidden,               // 403: Forbidden
	http.StatusNotFound,                // 404: Not Found
	http.StatusInternalServerError,     // 500: Internal Server Error
	http.StatusNotImplemented,          // 501: Not Implemented
	http.StatusServiceUnavailable,      // 503: Service Unavailable
}

// handleEvents is the handler function for all requests to /events/.
func handleEvents(w http.ResponseWriter, r *http.Request) {
	// Select a random index from the statusCodes slice
	index := rand.Intn(len(statusCodes))
	statusCode := statusCodes[index]

	// Set the status code and write the header
	// This must be called before w.Write() if you want to set a non-200 status.
	w.WriteHeader(statusCode)
}

func main() {
	// Seed the random number generator using the current time for varying results
	rand.Seed(time.Now().UnixNano())

	// Define the handler for the /events/ endpoint (will match /events/ and /events/anything)
	http.HandleFunc("/events/", handleEvents)

	port := ":9999"
	fmt.Printf("Starting Random Status Server on http://localhost%s\n", port)
	fmt.Printf("Test the endpoint: curl http://localhost%s/events/ -I\n\n", port)

	// Start the server
	if err := http.ListenAndServe(port, nil); err != nil {
		fmt.Printf("Server failed to start: %v\n", err)
	}
}