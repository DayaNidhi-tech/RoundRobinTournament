window.authReady = (async () => {
    const token = sessionStorage.getItem('authToken');
    if (!token) {
        location.replace('login.html');
        return null;
    }
    try {
        return await API.get('/api/auth/me');
    } catch (error) {
        toast(error.message, true);
        return null;
    }
})();

document.addEventListener('DOMContentLoaded', async () => {
    const user = await window.authReady;
    if (!user) {
        return;
    }
    window.currentUser = user;
    const initial = (user.name || user.username).charAt(0).toUpperCase();

    document.querySelectorAll('.sidebar-footer strong, .profile-mini .profile-name, .profile-button strong')
        .forEach(element => element.textContent = user.name);
    document.querySelectorAll('.sidebar-footer span, .profile-mini .profile-role, .profile-button small, .profile-button span')
        .forEach(element => element.textContent = user.role === 'admin' ? 'Administrator' : 'View only');
    document.querySelectorAll('.admin-avatar, .profile-avatar')
        .forEach(element => element.textContent = initial);

    if (user.role === 'admin') {
        const management = document.querySelector('.sidebar-bottom .nav, .sidebar-bottom .sidebar-nav') ||
            document.querySelector('.sidebar-nav:last-of-type');
        if (management && !management.querySelector('[data-users-link]')) {
            const link = document.createElement('a');
            link.href = 'users.html';
            link.className = 'nav-item';
            link.dataset.usersLink = 'true';
            link.innerHTML = '<span class="nav-icon">U</span><span>Users</span>';
            management.append(link);
        }
    } else {
        const hideControls = () => {
            document.querySelectorAll('button:not(.round-tab):not(.auth-logout)')
                .forEach(button => button.hidden = true);
        };
        hideControls();
        new MutationObserver(hideControls).observe(document.body, {
            childList: true,
            subtree: true
        });
        document.querySelectorAll('[href="users.html"], [href="settings.html"]')
            .forEach(link => link.hidden = true);
        if (location.pathname.endsWith('/users.html')) {
            location.replace('index.html');
            return;
        }
    }

    const footer = document.querySelector('.sidebar-footer, .profile-mini');
    if (footer && !footer.querySelector('[data-logout]')) {
        const logout = document.createElement('button');
        logout.type = 'button';
        logout.className = 'btn alt auth-logout';
        logout.dataset.logout = 'true';
        logout.textContent = 'Sign out';
        logout.addEventListener('click', async () => {
            logout.disabled = true;
            try {
                await API.post('/api/auth/logout');
            } catch (error) {
                toast(error.message, true);
            } finally {
                sessionStorage.removeItem('authToken');
                location.replace('login.html');
            }
        });
        footer.append(logout);
    }
});
