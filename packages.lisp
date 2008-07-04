
(in-package #:cl-user)

(defpackage :cl-hurd.common
  (:nicknames :hurd-common)
  (:use :cl :cffi :tg)
  (:export :largest-representable-number
	   :num-bits
	   :define-helper-library
	   :define-stub-library
	   :error->string
	   :err
	   :unless-return
	   :translate-foreign-list
	   :with-gensyms
	   :select-error
	   :open-flags-t
	   :disable
	   :disabled
	   :enable
	   :disabled
	   :is
	   :read
	   :write
	   :exec
	   :norw
	   :largefile
	   :excl
	   :creat
	   :nolink
	   :notrans
	   :nofollow
	   :directory
	   :append
	   :async
	   :fsync
	   :sync
	   :noatime
	   :shlock
	   :exlock
	   :dsync
	   :rsync
	   :nonblock
	   :hurd
	   :trunc
	   :cloexec
	   :mode-t
	   :mode-bits
	   :stat
	   :stat-struct
	   :make-stat
	   :stat-t
	   :ptr
	   :fstype
	   :fsid
	   :ino
	   :gen
	   :rdev
	   :mode
	   :nlink
	   :uid
	   :gid
	   :size
	   :atime
	   :mtime
	   :ctime
	   :blksize
	   :blocks
	   :author
	   :flags
	   :set-trans
	   :set-type
	   :set-vtx
	   :stat-get
	   :stat-set
	   :open-flags
	   :with-cleanup
	   :getuid
	   :getgid
	   :geteuid
	   :getegid
	   :has-perms
	   :set-perms
	   :set-perms-if
	   :read
	   :write
	   :exec
	   :owner
	   :group
	   :others
	   :unknown
	   :reg
	   :lnk
	   :dir
	   :chr
	   :blk
	   :sock
	   :is-dir-p
	   :is-reg-p
	   :is-lnk-p
	   :is-chr-p
	   :is-blk-p
	   :is-sock-p
	   :is-fifo-p
	   :has-passive-trans-p
	   :memcpy
	   :set-active-trans
	   :set-passive-trans
	   :set-root
	   :copy-stat-struct
	   :only
	   :open-modes
	   :set-types
	   :set-spare
	   :gid-t
	   :uid-t
	   :valid-id-p
	   :time-value-t
	   :time-value
	   :split-path
	   :join-path
	   :link-max
	   :max-canon
	   :max-input
	   :name-max
	   :path-max
	   :pipe-buf
	   :chown-restricted
	   :no-trunc
	   :vdisable
	   :sync-io
	   :prio-io
	   :sock-maxbuf
	   :filesizebits
	   :rec-incr-xfer-size
	   :rec-max-xfer-size
	   :rec-min-xfer-size
	   :rec-xfer-align
	   :alloc-size-min
	   :symlink-max
	   :2-symlinks
	   :pathconf-type
	   :dirent-type
	   :unknown
	   :fifo
	   :chr
	   :dir
	   :blk
	   :reg
	   :lnk
	   :sock
	   :wht
	   :off-t
	   :ino-t
	   :pid-t))

(defpackage :cl-mach
  (:nicknames :mach)
  (:use :cl :cffi :hurd-common)
  (:export :task-self
		   :with-port-deallocate
		   :with-port-destroy
		   :with-port
		   :with-send-right
		   :with-receive-right
		   :port
		   :is
		   :port-pointer
		   :port-valid
		   :port-allocate
		   :port-deallocate
		   :port-destroy
		   :port-mod-refs
		   :port-move-member
		   :port-insert-right
		   :port-type
		   :port-request-notification
		   :port-mscount
		   :msg-seqno
		   :get-bootstrap-port
		   :msg-type-name
		   :msg-type-number
		   :msg-server-timeout
		   :maptime-map
		   :vm-size
		   :mmap-prot-flags
		   :mmap-map-flags
		   :prot-none
		   :prot-read
		   :prot-write
		   :prot-exec
		   :map-shared
		   :map-private
		   :map-anon
		   :mmap
		   :munmap
		   :round-page
		   :vm-allocate
		   :vm-address))

(defpackage :cl-hurd
  (:nicknames :hurd)
  (:use :cl :cffi :mach :hurd-common :tg)
  (:export :getauth
	   :get-send-right
	   :get-right
	   :port-right
	   :fsys-startup
	   :fsys-getroot
	   :make-bucket
	   :run-server
	   :add-port
	   :add-new-port
	   :has-port
	   :lookup-port
	   :+servers+
	   :+servers-exec+
	   :file-name-lookup
	   :touch
	   :io-stat
	   :io-server-version
	   :make-iouser
	   :make-iouser-mem
	   :iouser
	   :retry-type
	   :box-translated-p
	   :make-transbox
	   :port-info
	   :define-hurd-interface
	   :notify-server
	   ))

(defpackage :cl-hurd.translator
  (:nicknames :hurd-translator)
  (:use :cl :cffi :mach :hurd-common :hurd :tg :flexi-streams)
  (:export :translator
		   :make-root-node
		   :pathconf
		   :allow-open
		   :node
		   :get-translator
		   :file-chmod
		   :file-chown
		   :file-utimes
		   :dir-lookup
		   :create-file
		   :number-of-entries
		   :number-of-entries
		   :get-entries
		   :allow-author-change
		   :create-directory
		   :remove-entry
		   :read-file
		   :run-translator
		   :define-callback
		   :create-translator
		   :make-dirent
		   :propagate-read-to-execute
		   :file-sync
		   :file-syncfs
		   :name))

(defpackage :cl-hurd.translator.tree
  (:nicknames :hurd-tree-translator)
  (:use :cl :hurd-common :mach :hurd :hurd-translator) ;:cl-containers)
  (:export :fill-root-node
		   :tree-translator
		   :dir-entry
		   :entry
		   :add-entry
		   :make-dir
		   :make-entry
		   :get-entry
		   :setup-entry))

(defpackage :cl-hurd.translator.examples
  (:nicknames :hurd-example-translators)
  (:use :cl :hurd-common :mach
		:hurd :hurd-translator
		:hurd-tree-translator
		:zip
		:flexi-streams))
