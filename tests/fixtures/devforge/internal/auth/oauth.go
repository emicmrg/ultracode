package auth

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"net/url"
	"strings"
	"time"

	"github.com/devforge/platform/internal/models"
)

type OAuth2Client struct {
	config  OAuth2Config
	client  *http.Client
	timeout time.Duration
}

type OAuth2Config struct {
	Provider     models.OAuthProvider
	RedirectURL  string
	State        string
}

func NewOAuth2Client(cfg OAuth2Config) *OAuth2Client {
	return &OAuth2Client{
		config:  cfg,
		client:  &http.Client{Timeout: 30 * time.Second},
		timeout: 30 * time.Second,
	}
}

func (c *OAuth2Client) NewProvider(name, clientID, clientSecret, authURL, tokenURL string, scopes []string) *models.OAuthProvider {
	return &models.OAuthProvider{
		Name:         name,
		ClientID:     clientID,
		ClientSecret: clientSecret,
		AuthURL:      authURL,
		TokenURL:     tokenURL,
		Scopes:       scopes,
	}
}

func (c *OAuth2Client) Authorize() (string, error) {
	params := url.Values{}
	params.Set("client_id", c.config.Provider.ClientID)
	params.Set("redirect_uri", c.config.RedirectURL)
	params.Set("response_type", "code")
	params.Set("state", c.config.State)
	if len(c.config.Provider.Scopes) > 0 {
		params.Set("scope", strings.Join(c.config.Provider.Scopes, " "))
	}
	return c.config.Provider.AuthURL + "?" + params.Encode(), nil
}

func (c *OAuth2Client) Exchange(ctx context.Context, code string) (map[string]interface{}, error) {
	data := url.Values{}
	data.Set("grant_type", "authorization_code")
	data.Set("code", code)
	data.Set("redirect_uri", c.config.RedirectURL)
	data.Set("client_id", c.config.Provider.ClientID)
	data.Set("client_secret", c.config.Provider.ClientSecret)

	req, err := http.NewRequestWithContext(ctx, http.MethodPost, c.config.Provider.TokenURL, strings.NewReader(data.Encode()))
	if err != nil {
		return nil, err
	}
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	req.Header.Set("Accept", "application/json")

	resp, err := c.client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("oauth exchange failed: status %d", resp.StatusCode)
	}

	var result map[string]interface{}
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return nil, err
	}
	return result, nil
}

func (c *OAuth2Client) Validate(state string) error {
	if state != c.config.State {
		return fmt.Errorf("state mismatch: expected %s", c.config.State)
	}
	if state == "" {
		return fmt.Errorf("empty state")
	}
	return nil
}
