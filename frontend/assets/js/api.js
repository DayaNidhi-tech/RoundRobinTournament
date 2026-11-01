const API = {
    async request(path, options = {}) {
        const headers = new Headers(options.headers || {});
        if (options.body !== undefined) {
            headers.set('Content-Type', 'application/json');
        }
        const token = sessionStorage.getItem('authToken');
        if (token) {
            headers.set('Authorization', `Bearer ${token}`);
        }

        const response = await fetch(path, { ...options, headers });
        let result;
        try {
            result = await response.json();
        } catch {
            throw Error(`The server returned an invalid response (${response.status}).`);
        }

        if (!response.ok || !result.success) {
            if (
                response.status === 401 &&
                !path.startsWith('/api/auth/login') &&
                !path.startsWith('/api/auth/setup') &&
                !path.startsWith('/api/auth/status')
            ) {
                sessionStorage.removeItem('authToken');
                location.replace('login.html');
            }
            throw Error(result.message || `Request failed (${response.status}).`);
        }
        return result.data ?? result;
    },
    get(path) {
        return this.request(path);
    },
    post(path, data = {}) {
        return this.request(path, { method: 'POST', body: JSON.stringify(data) });
    },
    put(path, data) {
        return this.request(path, { method: 'PUT', body: JSON.stringify(data) });
    },
    delete(path) {
        return this.request(path, { method: 'DELETE' });
    },
    records(data, fields, label) {
        if (!Array.isArray(data)) {
            throw Error(`${label} response is malformed.`);
        }
        for (const item of data) {
            for (const field of fields) {
                if (!Object.prototype.hasOwnProperty.call(item, field)) {
                    throw Error(`${label} response is missing '${field}'.`);
                }
            }
        }
        return data;
    }
};
