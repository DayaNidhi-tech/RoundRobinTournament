document.addEventListener('DOMContentLoaded', () => {
    document.querySelector('#addUser').addEventListener('click', createUser);
    document.querySelector('#userRows').addEventListener('click', handleUserAction);
    window.authReady.then(user => {
        if (user && user.role === 'admin') {
            window.currentUser = user;
            loadUsers();
        }
    });
});

function escapeHtml(value) {
    return String(value).replace(/[&<>"']/g, character => ({
        '&': '&amp;',
        '<': '&lt;',
        '>': '&gt;',
        '"': '&quot;',
        "'": '&#39;'
    })[character]);
}

async function loadUsers() {
    try {
        const users = API.records(
            await API.get('/api/users'),
            ['id', 'username', 'name', 'role'],
            'Users'
        );
        const currentId = window.currentUser && window.currentUser.id;
        document.querySelector('#userRows').innerHTML = users.map(user => `
            <tr>
                <td><b>${escapeHtml(user.name)}</b></td>
                <td>${escapeHtml(user.username)}</td>
                <td><span class="role-badge ${user.role === 'admin' ? 'admin' : ''}">${escapeHtml(user.role)}</span></td>
                <td class="inline-actions">
                    <button class="btn alt" type="button" data-action="edit" data-id="${user.id}">Edit</button>
                    <button class="btn danger" type="button" data-action="delete" data-id="${user.id}" ${user.id === currentId ? 'title="Delete your account and clear all saved data"' : ''}>${user.id === currentId ? 'Reset account' : 'Delete'}</button>
                </td>
            </tr>
        `).join('') || '<tr><td colspan="4" class="empty">No user accounts found.</td></tr>';
    } catch (error) {
        toast(error.message, true);
    }
}

function createUser() {
    modal(
        'Add user',
        `<div class="field"><label for="name">Full name</label><input id="name" maxlength="95" required></div>
         <div class="field"><label for="username">Username</label><input id="username" minlength="3" maxlength="63" pattern="[A-Za-z0-9._-]+" required></div>
         <div class="field"><label for="password">Temporary password</label><input id="password" type="password" minlength="8" maxlength="128" required></div>
         <div class="field"><label for="role">Role</label><select id="role"><option value="viewer">View only</option><option value="admin">Administrator</option></select></div>`,
        async () => {
            await API.post('/api/users', {
                name: $('#name').value,
                username: $('#username').value,
                password: $('#password').value,
                role: $('#role').value
            });
            toast('User account created');
            await loadUsers();
        }
    );
}

async function handleUserAction(event) {
    const button = event.target.closest('button[data-action]');
    if (!button) {
        return;
    }
    const userId = Number(button.dataset.id);
    if (button.dataset.action === 'edit') {
        try {
            const users = await API.get('/api/users');
            const user = users.find(item => item.id === userId);
            if (!user) {
                toast('User not found', true);
                return;
            }
            editUser(user);
        } catch (error) {
            toast(error.message, true);
        }
    } else if (button.dataset.action === 'delete') {
        const isCurrentUser = userId === Number(window.currentUser && window.currentUser.id);
        const confirmMessage = isCurrentUser
            ? 'This will delete your account and clear all saved tournaments and users. Continue?'
            : 'Delete this user account?';
        if (!confirm(confirmMessage)) {
            return;
        }
        try {
            if (isCurrentUser) {
                await API.delete('/api/auth/reset');
                sessionStorage.removeItem('authToken');
                location.replace('login.html');
                return;
            }
            await API.delete(`/api/users/${userId}`);
            toast('User account deleted');
            await loadUsers();
        } catch (error) {
            toast(error.message, true);
        }
    }
}

function editUser(user) {
    modal(
        `Edit ${escapeHtml(user.username)}`,
        `<div class="field"><label for="name">Full name</label><input id="name" maxlength="95" value="${escapeHtml(user.name)}" required></div>
         <div class="field"><label for="role">Role</label><select id="role">
            <option value="viewer" ${user.role === 'viewer' ? 'selected' : ''}>View only</option>
            <option value="admin" ${user.role === 'admin' ? 'selected' : ''}>Administrator</option>
         </select></div>
         <div class="field"><label for="password">Reset password (optional)</label><input id="password" type="password" minlength="8" maxlength="128" autocomplete="new-password"></div>`,
        async () => {
            const update = {
                name: $('#name').value,
                role: $('#role').value
            };
            if ($('#password').value) {
                update.password = $('#password').value;
            }
            await API.put(`/api/users/${user.id}`, update);
            toast('User account updated');
            await loadUsers();
        }
    );
}
