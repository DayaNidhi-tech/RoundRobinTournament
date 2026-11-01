document.addEventListener('DOMContentLoaded', async () => {
    const title = document.querySelector('#authTitle');
    const description = document.querySelector('#authDescription');
    const form = document.querySelector('#authForm');
    const error = document.querySelector('#authError');
    let setupRequired;

    if (sessionStorage.getItem('authToken')) {
        try {
            await API.get('/api/auth/me');
            location.replace('index.html');
            return;
        } catch {
            sessionStorage.removeItem('authToken');
        }
    }

    try {
        ({ setupRequired } = await API.get('/api/auth/status'));
    } catch (requestError) {
        error.textContent = requestError.message;
        return;
    }

    title.textContent = setupRequired ? 'Set up your administrator' : 'Welcome back';
    description.textContent = setupRequired
        ? 'Create the first administrator account to secure this Tournament Hub.'
        : 'Sign in to access your tournaments.';

    form.innerHTML = `
        ${setupRequired ? `
            <div class="field">
                <label for="name">Your name</label>
                <input id="name" name="name" autocomplete="name" required maxlength="95">
            </div>
        ` : ''}
        <div class="field">
            <label for="username">Username</label>
            <input id="username" name="username" autocomplete="username" required maxlength="63">
        </div>
        <div class="field">
            <label for="password">Password</label>
            <input id="password" name="password" type="password"
                autocomplete="${setupRequired ? 'new-password' : 'current-password'}"
                minlength="8" maxlength="128" required>
            ${setupRequired ? '<small class="muted">Use at least 8 characters.</small>' : ''}
        </div>
        <button class="btn auth-submit" type="submit">
            ${setupRequired ? 'Create administrator' : 'Sign in'}
        </button>
    `;

    form.addEventListener('submit', async event => {
        event.preventDefault();
        error.textContent = '';
        const submit = form.querySelector('[type="submit"]');
        submit.disabled = true;
        const payload = {
            username: form.elements.username.value,
            password: form.elements.password.value
        };
        if (setupRequired) {
            payload.name = form.elements.name.value;
        }

        try {
            const session = await API.post(
                setupRequired ? '/api/auth/setup' : '/api/auth/login',
                payload
            );
            sessionStorage.setItem('authToken', session.token);
            location.replace('index.html');
        } catch (requestError) {
            error.textContent = requestError.message;
            submit.disabled = false;
        }
    });
});
