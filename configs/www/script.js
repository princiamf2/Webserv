class WebservManager {
    constructor() {
        this.uploadedFiles = [];
        this.cgiRequestCount = 0;
        this.logs = [];
        this.currentResponse = null;
        this.init();
    }

    init() {
        this.setupTabs();
        this.setupFileUpload();
        this.setupCGITester();
        this.setupResponseTabs();
        this.checkServerStatus();
        setInterval(() => this.checkServerStatus(), 10000);
        this.log('info', 'Webserv Manager initialized');
    }

    // ==================== TAB MANAGEMENT ====================
    setupTabs() {
        const tabButtons = document.querySelectorAll('.tab-button');
        tabButtons.forEach(btn => {
            btn.addEventListener('click', () => {
                const tabName = btn.getAttribute('data-tab');
                this.switchTab(tabName);
            });
        });
    }

    switchTab(tabName) {
        document.querySelectorAll('.tab-content').forEach(content => {
            content.classList.remove('active');
        });
        document.querySelectorAll('.tab-button').forEach(btn => {
            btn.classList.remove('active');
        });

        document.getElementById(`${tabName}-tab`).classList.add('active');
        document.querySelector(`[data-tab="${tabName}"]`).classList.add('active');
        this.log('info', `Switched to ${tabName} tab`);
    }

    // ==================== FILE UPLOAD ====================
    setupFileUpload() {
        const dropZone = document.getElementById('dropZone');
        const fileInput = document.getElementById('fileInput');

        dropZone.addEventListener('click', () => fileInput.click());
        dropZone.addEventListener('dragover', (e) => this.handleDragOver(e));
        dropZone.addEventListener('dragleave', (e) => this.handleDragLeave(e));
        dropZone.addEventListener('drop', (e) => this.handleDrop(e));

        fileInput.addEventListener('change', (e) => this.handleFileSelect(e.target.files));

        document.addEventListener('dragover', (e) => e.preventDefault());
        document.addEventListener('drop', (e) => e.preventDefault());
    }

    handleDragOver(e) {
        e.preventDefault();
        e.stopPropagation();
        document.getElementById('dropZone').classList.add('dragover');
    }

    handleDragLeave(e) {
        e.preventDefault();
        e.stopPropagation();
        document.getElementById('dropZone').classList.remove('dragover');
    }

    handleDrop(e) {
        e.preventDefault();
        e.stopPropagation();
        document.getElementById('dropZone').classList.remove('dragover');
        this.handleFileSelect(e.dataTransfer.files);
    }

    handleFileSelect(files) {
        const filesArray = Array.from(files);
        if (filesArray.length === 0) return;

        document.getElementById('progressSection').style.display = 'block';

        filesArray.forEach((file, index) => {
            setTimeout(() => this.uploadFile(file), index * 200);
        });
    }

    uploadFile(file) {
        const formData = new FormData();
        formData.append('file', file);

        const fileItem = this.createFileItem(file);
        const fileItemElement = fileItem.element;

        if (document.querySelector('.file-list .empty-state')) {
            document.getElementById('fileList').innerHTML = '';
        }
        document.getElementById('fileList').insertBefore(fileItemElement, document.getElementById('fileList').firstChild);

        const xhr = new XMLHttpRequest();

        xhr.upload.addEventListener('progress', (e) => {
            if (e.lengthComputable) {
                const percentComplete = (e.loaded / e.total) * 100;
                this.updateFileProgress(fileItemElement, percentComplete);
            }
        });

        xhr.addEventListener('load', () => {
            if (xhr.status === 200 || xhr.status === 201) {
                this.markFileAsSuccess(fileItemElement, fileItem);
                this.uploadedFiles.push(fileItem);
                this.updateStatistics();

                const locationHeader = xhr.getResponseHeader('Location');
                const responseText = xhr.responseText;
                fileItem.uploadPath = locationHeader;

                this.log('success', `File uploaded: ${file.name}`);
                if (locationHeader) {
                    this.log('info', `Location: ${locationHeader}`);
                    this.addUploadLocation(file.name, locationHeader, responseText);
                }
            } else {
                this.markFileAsError(fileItemElement, `Error ${xhr.status}`);
                this.log('error', `Upload failed: ${file.name} (${xhr.status})`);
            }
        });

        xhr.addEventListener('error', () => {
            this.markFileAsError(fileItemElement, 'Network error');
            this.log('error', `Network error uploading ${file.name}`);
        });

        const uploadUrl = '/upload/';
        xhr.open('POST', uploadUrl, true);
        xhr.setRequestHeader('X-File-Upload', 'true');

        try {
            xhr.send(formData);
            this.markFileAsUploading(fileItemElement);
        } catch (error) {
            this.markFileAsError(fileItemElement, 'Failed to send');
            this.log('error', `Upload error: ${error.message}`);
        }
    }

    createFileItem(file) {
        const element = document.createElement('div');
        element.className = 'file-item';
        element.innerHTML = `
            <div class="file-info-content">
                <div class="file-name">${this.escapeHtml(file.name)}</div>
                <div class="file-meta">
                    <span>${this.formatFileSize(file.size)}</span>
                    <span>•</span>
                    <span class="upload-time">Uploading...</span>
                </div>
            </div>
            <div class="file-actions">
                <span class="file-status uploading">
                    <span class="spinner"></span>
                    <span>0%</span>
                </span>
                <button class="delete-btn">Delete</button>
            </div>
        `;

        element.querySelector('.delete-btn').addEventListener('click', () => {
            this.deleteFile(file.name, element);
        });

        return { element, file, startTime: new Date(), uploadedBytes: 0 };
    }

    updateFileProgress(fileElement, percent) {
        const statusBadge = fileElement.querySelector('.file-status span:last-child');
        if (statusBadge) statusBadge.textContent = `${Math.round(percent)}%`;
    }

    markFileAsUploading(fileElement) {
        const status = fileElement.querySelector('.file-status');
        if (status) {
            status.className = 'file-status uploading';
            status.innerHTML = '<span class="spinner"></span><span>0%</span>';
        }
    }

    markFileAsSuccess(fileElement, fileItem) {
        fileElement.classList.add('success');
        const status = fileElement.querySelector('.file-status');
        if (status) {
            status.className = 'file-status success';
            status.innerHTML = '✓ Uploaded';
        }
        const timeSpan = fileElement.querySelector('.upload-time');
        if (timeSpan) {
            const duration = ((new Date() - fileItem.startTime) / 1000).toFixed(1);
            timeSpan.textContent = `Uploaded in ${duration}s`;
        }
    }

    markFileAsError(fileElement, error) {
        fileElement.classList.add('error');
        const status = fileElement.querySelector('.file-status');
        if (status) {
            status.className = 'file-status error';
            status.innerHTML = `✗ ${error}`;
        }
    }

    deleteFile(filename, fileElement) {
        const fileItem = this.uploadedFiles.find(item => item.element === fileElement);

        if (!fileItem || !fileItem.uploadPath) {
            this.log('error', 'No upload path found for delete');
            return;
        }

        fetch(fileItem.uploadPath, { method: 'DELETE' })
            .then(response => {
                if (!response.ok) {
                    throw new Error(`DELETE failed: ${response.status}`);
                }

                fileElement.style.animation = 'slideIn 0.3s ease reverse';
                setTimeout(() => {
                    fileElement.remove();
                    this.uploadedFiles = this.uploadedFiles.filter(item => item.element !== fileElement);

                    if (this.uploadedFiles.length === 0) {
                        document.getElementById('fileList').innerHTML = '<p class="empty-state">No files uploaded yet</p>';
                    }

                    this.updateStatistics();
                }, 300);

                this.log('success', `Deleted: ${fileItem.uploadPath}`);
            })
            .catch(error => this.log('error', `Failed to delete file: ${error.message}`));
    }

    addUploadLocation(filename, locationHeader, responseBody) {
        const uploadLocations = document.getElementById('uploadLocations');

        if (uploadLocations.querySelector('.empty-state')) {
            uploadLocations.innerHTML = '';
        }

        const locationItem = document.createElement('div');
        locationItem.className = 'upload-location-item';

        const timestamp = new Date().toLocaleString();

        locationItem.innerHTML = `
            <strong>📄 ${this.escapeHtml(filename)}</strong>
            <div class="upload-location-path">Location: ${this.escapeHtml(locationHeader)}</div>
            <div class="upload-location-meta">
                <span>Uploaded: ${timestamp}</span><br>
                <span>Response: ${this.escapeHtml(responseBody.substring(0, 50))}...</span>
            </div>
            <button class="location-copy-btn" onclick="navigator.clipboard.writeText('${locationHeader.replace(/'/g, "\\'")}'); alert('Copied!')">📋 Copy Path</button>
        `;

        uploadLocations.insertBefore(locationItem, uploadLocations.firstChild);
    }

    // ==================== CGI TESTER ====================
    setupCGITester() {
        window.addHeaderRow = () => {
            const container = document.getElementById('headersEditor');
            const newRow = document.createElement('div');
            newRow.className = 'header-row';
            newRow.innerHTML = `
                <input type="text" placeholder="Header Name" class="header-key">
                <input type="text" placeholder="Header Value" class="header-value">
                <button class="btn-remove" onclick="this.parentElement.remove()">✕</button>
            `;
            container.appendChild(newRow);
        };

        window.addEnvRow = () => {
            const container = document.getElementById('envEditor');
            const newRow = document.createElement('div');
            newRow.className = 'env-row';
            newRow.innerHTML = `
                <input type="text" placeholder="Variable Name" class="env-key">
                <input type="text" placeholder="Variable Value" class="env-value">
                <button class="btn-remove" onclick="this.parentElement.remove()">✕</button>
            `;
            container.appendChild(newRow);
        };

        window.executeCGI = () => this.executeCGI();
        window.clearCGIForm = () => this.clearCGIForm();
        window.saveCGIPreset = () => this.saveCGIPreset();
        window.copyResponse = () => this.copyResponse();
    }

    executeCGI() {
        const scriptPath = document.getElementById('scriptPath').value;
        const method = document.getElementById('httpMethod').value;
        const queryParams = document.getElementById('queryParams').value;
        const requestBody = document.getElementById('requestBody').value;
        const bodyType = document.querySelector('input[name="bodyType"]:checked').value;

        if (!scriptPath) {
            this.log('error', 'Please enter a script path');
            alert('Please enter a script path');
            return;
        }

        let url = scriptPath;
        if (queryParams && method === 'GET') {
            url += '?' + queryParams;
        }

        const headers = { 'Content-Type': 'application/json' };
        document.querySelectorAll('.header-row').forEach(row => {
            const key = row.querySelector('.header-key').value;
            const value = row.querySelector('.header-value').value;
            if (key && value) headers[key] = value;
        });

        let body = null;
        if (method !== 'GET' && requestBody) {
            if (bodyType === 'json') {
                headers['Content-Type'] = 'application/json';
                body = requestBody;
            } else if (bodyType === 'form') {
                headers['Content-Type'] = 'application/x-www-form-urlencoded';
                body = requestBody;
            } else {
                body = requestBody;
            }
        }

        const startTime = performance.now();
        this.log('info', `Executing CGI: ${method} ${url}`);

        const btn = document.getElementById('executeCgiBtn');
        btn.disabled = true;
        btn.innerHTML = '<span class="loading-spinner"></span> Executing...';

        fetch(url, {
            method,
            headers,
            body
        })
            .then(response => {
                const endTime = performance.now();
                const duration = (endTime - startTime).toFixed(2);

                this.currentResponse = {
                    status: response.status,
                    statusText: response.statusText,
                    headers: response.headers,
                    duration
                };

                this.updateResponseDisplay(response, duration);
                this.log('success', `CGI executed: ${response.status} ${response.statusText} (${duration}ms)`);

                return response.text();
            })
            .then(body => {
                document.getElementById('responseBody').innerHTML = `<div class="response-display"><pre>${this.escapeHtml(body)}</pre></div>`;
            })
            .catch(error => {
                this.log('error', `CGI Error: ${error.message}`);
                document.getElementById('responseBody').innerHTML = `<div class="response-display" style="color: #ef4444;"><pre>Error: ${this.escapeHtml(error.message)}</pre></div>`;
            })
            .finally(() => {
                btn.disabled = false;
                btn.innerHTML = '▶ Execute CGI';
                this.cgiRequestCount++;
                this.updateStatistics();
            });
    }

    updateResponseDisplay(response, duration) {
        const statusElement = document.getElementById('statusCode');
        statusElement.textContent = response.status;
        statusElement.className = 'status-badge';
        if (response.status >= 200 && response.status < 300) {
            statusElement.classList.add('success');
        } else if (response.status >= 400) {
            statusElement.classList.add('error');
        } else if (response.status >= 300) {
            statusElement.classList.add('warning');
        }

        document.getElementById('responseTime').textContent = `${duration}ms`;

        const headersHtml = Array.from(response.headers.entries())
            .map(([key, value]) => `<li><span class="header-name">${this.escapeHtml(key)}:</span> <span class="header-value">${this.escapeHtml(value)}</span></li>`)
            .join('');
        document.getElementById('responseHeaders').innerHTML = headersHtml ? `<ul>${headersHtml}</ul>` : '<p class="empty-state">No headers</p>';

        document.getElementById('responseTiming').innerHTML = `
            <ul>
                <li><span class="timing-label">Response Time:</span> <span class="timing-value">${duration}ms</span></li>
                <li><span class="timing-label">Status:</span> <span class="timing-value">${response.status} ${response.statusText}</span></li>
            </ul>
        `;
    }

    clearCGIForm() {
        document.getElementById('scriptPath').value = '';
        document.getElementById('queryParams').value = '';
        document.getElementById('requestBody').value = '';
        document.querySelectorAll('.header-row:not(:first-child)').forEach(row => row.remove());
        document.querySelectorAll('.env-row:not(:first-child)').forEach(row => row.remove());
        this.log('info', 'CGI form cleared');
    }

    saveCGIPreset() {
        const preset = {
            cgiType: document.getElementById('cgiType').value,
            httpMethod: document.getElementById('httpMethod').value,
            scriptPath: document.getElementById('scriptPath').value,
            queryParams: document.getElementById('queryParams').value,
            requestBody: document.getElementById('requestBody').value,
            headers: {},
            env: {}
        };

        document.querySelectorAll('.header-row').forEach(row => {
            const key = row.querySelector('.header-key').value;
            const value = row.querySelector('.header-value').value;
            if (key) preset.headers[key] = value;
        });

        document.querySelectorAll('.env-row').forEach(row => {
            const key = row.querySelector('.env-key').value;
            const value = row.querySelector('.env-value').value;
            if (key) preset.env[key] = value;
        });

        localStorage.setItem('cgiPreset_' + Date.now(), JSON.stringify(preset));
        this.log('success', 'CGI preset saved');
        alert('Preset saved successfully!');
    }

    setupResponseTabs() {
        document.querySelectorAll('.response-tab').forEach(tab => {
            tab.addEventListener('click', () => {
                document.querySelectorAll('.response-tab').forEach(t => t.classList.remove('active'));
                document.querySelectorAll('.response-tab-content').forEach(c => c.classList.remove('active'));

                tab.classList.add('active');
                const tabName = tab.getAttribute('data-response');
                document.getElementById(`response-${tabName}`).classList.add('active');
            });
        });
    }

    copyResponse() {
        const responseBody = document.querySelector('.response-display pre')?.textContent;
        if (responseBody) {
            navigator.clipboard.writeText(responseBody);
            this.log('success', 'Response copied to clipboard');
            alert('Response copied!');
        }
    }

    // ==================== LOGGING ====================
    log(type, message) {
        const timestamp = new Date().toLocaleTimeString();
        this.logs.push({ type, message, timestamp });

        const logEntry = document.createElement('div');
        logEntry.className = 'log-entry';
        logEntry.innerHTML = `
            <span class="log-timestamp">${timestamp}</span>
            <span class="log-type ${type}">${type.toUpperCase()}</span>
            <span class="log-message">${this.escapeHtml(message)}</span>
        `;

        const logsDisplay = document.getElementById('logsDisplay');
        if (logsDisplay.querySelector('.empty-state')) {
            logsDisplay.innerHTML = '';
        }
        logsDisplay.appendChild(logEntry);

        if (document.getElementById('autoScroll').checked) {
            logsDisplay.scrollTop = logsDisplay.scrollHeight;
        }
    }

    // ==================== UTILITIES ====================
    formatFileSize(bytes) {
        if (bytes === 0) return '0 B';
        const k = 1024;
        const sizes = ['B', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return Math.round((bytes / Math.pow(k, i)) * 100) / 100 + ' ' + sizes[i];
    }

    escapeHtml(text) {
        const map = {
            '&': '&amp;',
            '<': '&lt;',
            '>': '&gt;',
            '"': '&quot;',
            "'": '&#039;'
        };
        return text.replace(/[&<>"']/g, m => map[m]);
    }

    updateStatistics() {
        document.getElementById('totalFiles').textContent = this.uploadedFiles.length;
        const totalSize = this.uploadedFiles.reduce((sum, item) => sum + item.file.size, 0);
        document.getElementById('totalSize').textContent = this.formatFileSize(totalSize);
        document.getElementById('cgiRequests').textContent = this.cgiRequestCount;
    }

    checkServerStatus() {
        fetch('/health', { method: 'HEAD' })
            .then(() => {
                const statusElement = document.getElementById('serverStatus');
                statusElement.textContent = '🟢 Connected';
                statusElement.classList.add('connected');
                statusElement.classList.remove('disconnected');
            })
            .catch(() => {
                const statusElement = document.getElementById('serverStatus');
                statusElement.textContent = '🔴 Disconnected';
                statusElement.classList.add('disconnected');
                statusElement.classList.remove('connected');
            });
    }
}

// Global functions for window access
window.addHeaderRow = () => window.manager?.addHeaderRow?.();
window.addEnvRow = () => window.manager?.addEnvRow?.();
window.executeCGI = () => window.manager?.executeCGI?.();
window.clearCGIForm = () => window.manager?.clearCGIForm?.();
window.saveCGIPreset = () => window.manager?.saveCGIPreset?.();
window.copyResponse = () => window.manager?.copyResponse?.();
window.clearLogs = () => {
    if (window.manager?.logs) {
        window.manager.logs = [];
        document.getElementById('logsDisplay').innerHTML = '<p class="empty-state">No logs yet</p>';
    }
};
window.downloadLogs = () => {
    if (window.manager?.logs && window.manager.logs.length > 0) {
        const logsText = window.manager.logs
            .map(log => `[${log.timestamp}] ${log.type.toUpperCase()}: ${log.message}`)
            .join('\n');
        const blob = new Blob([logsText], { type: 'text/plain' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `webserv-logs-${new Date().toISOString()}.txt`;
        a.click();
        URL.revokeObjectURL(url);
    }
};

// Initialize manager
document.addEventListener('DOMContentLoaded', () => {
    window.manager = new WebservManager();
});
