
(in-package :hurd-translator)

;;
;; This file implements the open-node class.
;; A open-node refers to a node that is open.
;;

(defclass open-node ()
  ((node :initarg :refers
         :accessor refers
         :documentation "Node this refers to.")
   (file-pos :initform 0
             :accessor file-offset
             :documentation "File offset.")
   (lock-status :initform 'unlock
                :documentation "Lock status.") ; /usr/include/sys/file.h
   (openstat :initform nil
             :initarg :flags
             :accessor flags
             :documentation "Open flags for this node.")
   (root-parent :initform nil
                :accessor root-parent
                :documentation "Port to the root parent.")
   (shadow-root :initform nil
                :accessor shadow-root
                :documentation "Shadow root.")
   (shadow-root-parent :initform nil
                       :accessor shadow-root-parent
                       :documentation "Shadow root parent."))
  (:documentation "Open node class."))

(defun make-open-node (node flags
                            &key (root-parent nil)
                            (shadow-root nil)
                            (shadow-root-parent nil))
  "Creates a new open node."
  (let ((obj (make-instance 'open-node :refers node :flags flags)))
    (setf (root-parent obj) root-parent)
    (if (port-valid (root-parent obj))
      (port-mod-refs (root-parent obj) :right-send 1))
    (setf (shadow-root obj) shadow-root)
    (setf (shadow-root-parent obj) shadow-root-parent)
    (if (port-valid shadow-root-parent)
      (port-mod-refs shadow-root-parent :right-send 1))
    obj))
