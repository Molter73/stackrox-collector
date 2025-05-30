package collector

import (
	"fmt"
	"io"
	"net/http"

	"github.com/stackrox/collector/integration-tests/pkg/log"
)

func IntrospectionQuery(collectorIP string, endpoint string) ([]byte, error) {
	uri := fmt.Sprintf("http://%s:8080%s", collectorIP, endpoint)
	resp, err := http.Get(uri)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, log.Error("IntrospectionQuery failed with %s", resp.Status)
	}

	body, err := io.ReadAll(resp.Body)
	return body, err
}
