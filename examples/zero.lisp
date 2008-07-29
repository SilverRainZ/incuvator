
(defpackage :zero-translator
  (:use :cl :hurd-common :mach
        :hurd :hurd-translator))

(in-package :zero-translator)

(defclass zero-translator (translator)
  ((name :initform "zero-translator"))
  (:documentation "The zero-translator."))

(define-callback make-root-node zero-translator
                 (underlying-stat)
  (let ((mode (make-mode :perms '((:owner :read :write)
                                  (:group :read :write)
                                  (:others :read :write))
                         :type :chr)))
    (make-instance 'node
                   :stat (make-stat underlying-stat
                                    :mode mode))))

(define-callback file-read zero-translator
                 (node user start amount stream)
  (declare (ignore translator node user start))
  (loop for i from 0 below amount
        do (write-byte 1 stream))
  t)

(define-callback file-write zero-translator
                 (node user offset stream)
  (declare (ignore translator node user offset))
  ; Empty the stream to look like we used it all.
  (loop while (read-byte stream nil))
  t)

(defun main ()
  (run-translator (make-instance 'zero-translator)))

(main)

