/*
 * Copyright 2017 NXP
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
package com.example.accelapp;

public class AwsShadow {
    public State state;
    public Metadata metadata;

    public AwsShadow() {
        state = new State();
        metadata = new Metadata();
    }

    /** Desired, reported and delta state of the shadow  */
    public class State {
        public Desired desired;
        public Reported reported;

        State() {
            desired = new Desired();
            reported = new Reported();
        }

        /** Desired state = what client wants to device to have set */
        public class Desired {
            Desired() {}

            /** Accelerometer update */
            public Integer accelUpdate;
            public Integer rpmUpdate;
            public Integer speedUpdate;
            public Integer mafUpdate;
            public Integer loadUpdate;
            public Integer osUpdate;
        }

        /** Reported state = what device has set */
        public class Reported {
            Reported() {}

            /** Accelerometer */
            public Accel accel;

            public class Accel {
                public Integer x;
                public Integer y;
                public Integer z;
            }

            /** RPM */
            public RPM rpm;

            public class RPM {
                public Integer rpm;
            }

            /** Speed */
            public Speed speed;

            public class Speed {
                public Integer speed;
            }

            /** MAF */
            public MAF maf;

            public class MAF {
                public Integer maf;
            }

            /** Engine Load */
            public LOAD load;

            public class LOAD {
                public Integer load;
            }

            /** Oxygen Sensor */
            public OS os;

            public class OS {
                public Integer os;
            }
        }
    }

    /** Metadata holds timestamps of shadow changes */
    public class Metadata {
        public Reported reported;

        public class Reported {
            Reported() {}

            public Accel accel;
            public RPM rpm;
            public Speed speed;
            public MAF maf;
            public LOAD load;
            public OS os;

            public class RPM {
                public Timestamp rpm;
            }

            public class Speed {
                public Timestamp speed;
            }

            public class MAF {
                public Timestamp maf;
            }

            public class LOAD {
                public Timestamp load;
            }

            public class OS {
                public Timestamp os;
            }

            public class Accel {
                public Timestamp x;
                public Timestamp y;
                public Timestamp z;
            }

            public class Timestamp {
                public Long timestamp;
            }
        }
    }

    /** Version of shadow */
    public Long version;
    /** Timestamp of shadow */
    public Long timestamp;
}
