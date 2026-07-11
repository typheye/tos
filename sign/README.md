# Local signing material

This directory is the default location for the local TOS development signing
key. CMake creates or loads:

- development-signing-key.pk8
- development-signing-key.pk8.pub

All files in this directory except this README are ignored by Git. Never force
add a private key. Production builds should pass TOS_SIGNING_KEY explicitly and
store the private key outside the source tree or in an HSM/KMS.
