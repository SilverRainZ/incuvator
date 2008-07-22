
(in-package :hurd)

(defcfun ("file_lock_stat" %file-lock-stat)
  err
  (file port)
  (mystatus :pointer)
  (otherstatus :pointer))

(defun file-lock-stat (file)
  (declare (type fixnum file))
  (with-foreign-pointer (mystatus (foreign-type-size 'lock-flags))
    (with-foreign-pointer (otherstatus (foreign-type-size 'lock-flags))
      (let ((err (%file-lock-stat file mystatus otherstatus)))
        (select-error err
                      (list
                        (mem-ref mystatus 'lock-flags)
                        (mem-ref otherstatus 'lock-flags)))))))
