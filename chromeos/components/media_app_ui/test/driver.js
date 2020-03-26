// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Host-side of web-driver like controller for sandboxed guest frames. */
class GuestDriver {
  /**
   * Sends a query to the guest that repeatedly runs a query selector until
   * it returns an element.
   *
   * @param {string} query the querySelector to run in the guest.
   * @param {string=} opt_property a property to request on the found element.
   * @param {Object=} opt_commands test commands to execute on the element.
   * @return Promise<string> JSON.stringify()'d value of the property, or
   *   tagName if unspecified.
   */
  async waitForElementInGuest(query, opt_property, opt_commands = {}) {
    /** @type {TestMessageQueryData} */
    const message = {testQuery: query, property: opt_property};

    const result = /** @type {TestMessageResponseData} */ (
        await guestMessagePipe.sendMessage(
            'test', {...message, ...opt_commands}));
    return result.testQueryResult;
  }
}

/** @implements FileSystemWritableFileStream */
class FakeWritableFileStream {
  constructor(/** !Blob= */ data = new Blob()) {
    this.data = data;

    /** @type {function(!Blob)} */
    this.resolveClose;

    this.closePromise = new Promise((/** function(!Blob) */ resolve) => {
      this.resolveClose = resolve;
    });
  }
  /** @override */
  async write(data) {
    const position = 0;  // Assume no seeks.
    this.data = new Blob([
      this.data.slice(0, position),
      data,
      this.data.slice(position + data.size),
    ]);
  }
  /** @override */
  async truncate(size) {
    this.data = this.data.slice(0, size);
  }
  /** @override */
  async close() {
    this.resolveClose(this.data);
  }
  /** @override */
  async seek(offset) {
    throw new Error('seek() not implemented.')
  }
}

/** @implements FileSystemHandle  */
class FakeFileSystemHandle {
  constructor() {
    this.isFile = true;
    this.isDirectory = false;
    this.name = '';
  }
  /** @override */
  async isSameEntry(other) {
    return this === other;
  }
  /** @override */
  async queryPermission(descriptor) {}
  /** @override */
  async requestPermission(descriptor) {}
}

/** @implements FileSystemFileHandle  */
class FakeFileSystemFileHandle extends FakeFileSystemHandle {
  constructor() {
    super();
    /** @type {?FakeWritableFileStream} */
    this.lastWritable;
  }
  /** @override */
  createWriter(options) {
    throw new Error('createWriter() deprecated.')
  }
  /** @override */
  async createWritable(options) {
    this.lastWritable = new FakeWritableFileStream();
    return this.lastWritable;
  }
  /** @override */
  async getFile() {
    return new File([], this.name);
  }
}

/** @implements FileSystemDirectoryHandle  */
class FakeFileSystemDirectoryHandle extends FakeFileSystemHandle {
  constructor() {
    super();
    this.isFile = false;
    this.isDirectory = true;
    this.name = 'fake-dir';
    /**
     * Internal state mocking file handles in a directory handle.
     * @type {!Array<!FakeFileSystemFileHandle>}
     */
    this.files = [];
    /**
     * Used to spy on the last deleted file.
     * @type {?FakeFileSystemFileHandle}
     */
    this.lastDeleted = null;
  }
  /**
   * Use to populate `FileSystemFileHandle`s for tests.
   * @param {!FakeFileSystemFileHandle} fileHandle
   */
  addFileHandleForTest(fileHandle) {
    this.files.push(fileHandle);
  }
  /** @override */
  getFile(name, options) {
    const fileHandler = this.files.find(f => f.name === name);
    return fileHandler ? Promise.resolve(fileHandler) :
                         Promise.reject(new Error(`File ${name} not found`));
  }
  /** @override */
  getDirectory(name, options) {}
  /**
   * @override
   * @return {!AsyncIterable<!FileSystemHandle>}
   * @suppress {reportUnknownTypes} suppress [JSC_UNKNOWN_EXPR_TYPE] for `yield
   * file`.
   */
  async * getEntries() {
    for (const file of this.files) {
      yield file;
    }
  }
  /** @override */
  async removeEntry(name, options) {
    // Remove file handle from internal state.
    const fileHandleIndex = this.files.findIndex(f => f.name === name);
    // Store the file removed for spying in tests.
    this.lastDeleted = this.files.splice(fileHandleIndex, 1)[0];
  }
}

/** Creates a mock directory with a single file in it. */
function createMockTestDirectory() {
  const directory = new FakeFileSystemDirectoryHandle();
  directory.addFileHandleForTest(new FakeFileSystemFileHandle());
  return directory;
}

/**
 * Helper to send a single file to the guest.
 * @param {!File} file
 * @param {!FileSystemFileHandle} handle
 */
function loadFile(file, handle) {
  currentFiles.length = 0;
  currentFiles.push({token: -1, file, handle});
  entryIndex = 0;
  sendFilesToGuest();
}
