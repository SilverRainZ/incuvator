
(in-package :hurd-translator)

(defun %get-options-callback (port data data-len)
  (with-lookup protid port
    (let* ((args0 (get-options *translator*))
           (args (cons
                   (name *translator*) args0))
           (len-args (string-list-len args))
           (total (sum-list len-args)))
      (when (< (mem-ref data-len 'msg-type-number)
               total)
          (setf (mem-ref data :pointer)
                (mmap (make-pointer 0)
                      total
                      '(:prot-read :prot-write)
                      '(:map-anon)
                      0
                      0)))
      (setf (mem-ref data-len 'msg-type-number) total)
      (list-to-foreign-string-zero-separated args
                                             (mem-ref data :pointer)
                                             len-args)
      t)))
