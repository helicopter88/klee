FROM helicopter88/klee:base

# RUN mkdir ${KLEE_SRC}
ADD / ${KLEE_SRC}

# Build KLEE (use TravisCI script)
RUN cd ${BUILD_DIR} && ${KLEE_SRC}/.travis/klee.sh

# Revoke password-less sudo and Set up sudo access for the ``klee`` user so it
# requires a password
USER root
RUN mv /etc/sudoers.bak /etc/sudoers && \
    echo 'klee  ALL=(root) ALL' >> /etc/sudoers
RUN cd ${BUILD_DIR}/klee && \
    make install
# FIXME: Shouldn't we just invoke the `install` target? This will
# duplicate some files but the Docker image is already pretty bloated
# so this probably doesn't matter.
# Add KLEE binary directory to PATH
# RUN echo 'export PATH=$PATH:'${BUILD_DIR}'/klee/bin' >> /home/klee/.bashrc
RUN rm -Rf ${BUILD_DIR} ${KLEE_SRC}

# Link klee to /usr/bin so that it can be used by docker run
# USER root
# RUN for executable in ${BUILD_DIR}/klee/bin/* ; do cp ${executable} /usr/bin/`basename ${executable}`; done
USER klee
